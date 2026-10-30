/*
   Copyright (c) 2026 Filip Pizlo. All Rights Reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
   OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "filc_runtime.h"

#if PAS_ENABLE_FILC

#include "filc_native.h"
#include "filc_runtime_inlines.h"
#include "fugc.h"
#include "jit_heap.h"
#include <dlfcn.h>
#include <sched.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* We only support the LLVM JIT in glibc, because otherwise, there's no way to load the LLVM so,
   which is built against glibc.

   What a hack LMAO! */
#if PAS_GLIBC

#include "../../../filc/llvmjit/filc_llvm_jit_api.h"

bool filc_allow_jit = false;

static const bool verbose = false;

static const struct shim_api* api = NULL;
static pas_lock api_mutex = PAS_LOCK_INITIALIZER;

static void ensure_api_slow(filc_thread* my_thread);
static void ensure_api(filc_thread* my_thread)
{
    FILC_CHECK(filc_allow_jit, NULL,
               "Fil-C LLVM JIT API used but JIT support is not enabled for this process.");
    if (!api)
        ensure_api_slow(my_thread);
    pas_fence();
}

static void drop_context_state(filc_llvm_jit_context_object* object)
{
    if (!object)
        return;
    unsigned prev = __atomic_fetch_sub(&object->ref_count, 1u, __ATOMIC_SEQ_CST);
    if (prev != 1)
        return;
    if (object->handle)
        api->LLVMContextDispose((LLVMContextRef)object->handle);
}

static filc_ptr wrap_handle(filc_thread* my_thread,
                            filc_llvm_jit_context_object* context_obj,
                            filc_llvm_jit_object* module_obj,
                            filc_llvm_jit_kind kind,
                            void* handle)
{
    if (context_obj)
        PAS_ASSERT(context_obj->kind == FILC_LLVM_JIT_KIND_CONTEXT);
    if (module_obj)
        PAS_ASSERT(module_obj->kind == FILC_LLVM_JIT_KIND_MODULE);
    size_t size = SIZE_MAX;
    pas_heap* heap = NULL;
    switch (kind) {
    case FILC_LLVM_JIT_KIND_MODULE:
        size = sizeof(filc_llvm_jit_module_object);
        heap = fugc_destructor_heap;
        break;
    case FILC_LLVM_JIT_KIND_METADATA_ENTRIES:
    case FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES:
        size = sizeof(filc_llvm_jit_entries_object);
        heap = fugc_destructor_heap;
        break;
    case FILC_LLVM_JIT_KIND_BUILDER:
        size = sizeof(filc_llvm_jit_builder_object);
        heap = fugc_destructor_heap;
        break;
    case FILC_LLVM_JIT_KIND_OPERAND_BUNDLE:
        size = sizeof(filc_llvm_jit_value_object);
        heap = fugc_destructor_heap;
        break;
    case FILC_LLVM_JIT_KIND_TYPE:
    case FILC_LLVM_JIT_KIND_VALUE:
    case FILC_LLVM_JIT_KIND_BASIC_BLOCK:
    case FILC_LLVM_JIT_KIND_METADATA:
    case FILC_LLVM_JIT_KIND_NAMED_METADATA:
    case FILC_LLVM_JIT_KIND_USE:
    case FILC_LLVM_JIT_KIND_DBG_RECORD:
    case FILC_LLVM_JIT_KIND_ATTRIBUTE:
        size = sizeof(filc_llvm_jit_value_object);
        heap = fugc_default_heap;
        break;
    default:
        PAS_ASSERT(!"Bad kind");
    }
    PAS_ASSERT(size != SIZE_MAX);
    PAS_ASSERT(heap);
    if (!handle)
        return filc_ptr_forge_null();
    FILC_CHECK(context_obj, NULL,
               "Fil-C LLVM JIT API: null LLVM context.");
    switch (kind) {
    case FILC_LLVM_JIT_KIND_MODULE:
    case FILC_LLVM_JIT_KIND_BUILDER:
        __atomic_fetch_add(&context_obj->ref_count, 1u, __ATOMIC_SEQ_CST);
        break;
    default:
        break;
    }
    filc_object* object = filc_allocate_special_with_heap(
        my_thread, size, 1, FILC_SPECIAL_TYPE_LLVM_JIT, heap);
    filc_llvm_jit_object* payload = (filc_llvm_jit_object*)filc_object_lower(object);
    if (kind == FILC_LLVM_JIT_KIND_MODULE)
        module_obj = payload;
    filc_store_barrier(my_thread, filc_object_for_special_payload(context_obj));
    filc_store_barrier(my_thread, filc_object_for_special_payload(module_obj));
    payload->kind = kind;
    payload->context = context_obj;
    payload->module = (filc_llvm_jit_module_object*)module_obj;
    payload->handle = handle;
    pas_store_store_fence();
    return filc_ptr_create_with_special_object(my_thread, object);
}

static void builder_adopt_module(filc_thread* my_thread, filc_llvm_jit_object* builder,
                                 filc_llvm_jit_object* module_obj)
{
    PAS_ASSERT(builder);
    PAS_ASSERT(module_obj);
    PAS_ASSERT(builder->kind == FILC_LLVM_JIT_KIND_BUILDER);
    PAS_ASSERT(module_obj->kind == FILC_LLVM_JIT_KIND_MODULE);
    filc_llvm_jit_builder_object* b = (filc_llvm_jit_builder_object*)builder;

    if (b->base.module) {
        FILC_CHECK(&b->base.module->base == module_obj, NULL,
                   "Fil-C LLVM JIT API: cannot move builder between modules");
        return;
    }

    filc_store_barrier(my_thread, filc_object_for_special_payload(module_obj));
    b->base.module = (filc_llvm_jit_module_object*)module_obj;
}

static filc_llvm_jit_object* unwrap_obj(filc_thread* my_thread, filc_ptr ptr,
                                        filc_llvm_jit_kind expected,
                                        filc_llvm_jit_context_object** context_inout,
                                        filc_llvm_jit_object** module_inout)
{
    PAS_UNUSED_PARAM(my_thread);
    filc_check_access_special(ptr, FILC_SPECIAL_TYPE_LLVM_JIT);
    filc_llvm_jit_object* payload = (filc_llvm_jit_object*)filc_ptr_ptr(ptr);
    FILC_CHECK(payload->kind == expected, NULL,
               "Fil-C LLVM JIT API: object kind mismatch.");
    if (!*context_inout)
        *context_inout = payload->context;
    else {
        FILC_CHECK(*context_inout == payload->context, NULL,
                   "Fil-C LLVM JIT API: mixing objects from different LLVM contexts.");
    }
    if (payload->module && module_inout) {
        if (!*module_inout)
            *module_inout = &payload->module->base;
        else {
            FILC_CHECK(*module_inout == &payload->module->base, NULL,
                       "Fil-C LLVM JIT API: mixing objects from different LLVM modules.");
        }
    }
    return payload;
}

static filc_llvm_jit_object* unwrap_obj_or_null(filc_thread* my_thread, filc_ptr ptr,
                                                filc_llvm_jit_kind expected,
                                                filc_llvm_jit_context_object** context_inout,
                                                filc_llvm_jit_object** module_inout)
{
    if (!filc_ptr_ptr(ptr))
        return NULL;
    return unwrap_obj(my_thread, ptr, expected, context_inout, module_inout);
}

static filc_llvm_jit_context_object* unwrap_context(filc_thread* my_thread, filc_ptr ptr)
{
    PAS_UNUSED_PARAM(my_thread);
    FILC_CHECK(filc_ptr_ptr(ptr), NULL,
               "Fil-C LLVM JIT API: null LLVM context.");
    filc_check_access_special(ptr, FILC_SPECIAL_TYPE_LLVM_JIT);
    filc_llvm_jit_context_object* payload = (filc_llvm_jit_context_object*)filc_ptr_ptr(ptr);
    FILC_CHECK(payload->kind == FILC_LLVM_JIT_KIND_CONTEXT, NULL,
               "Fil-C LLVM JIT API: expected an LLVM context object.");
    return payload;
}

static filc_llvm_jit_session_object* unwrap_session(filc_thread* my_thread, filc_ptr ptr)
{
    PAS_UNUSED_PARAM(my_thread);
    FILC_CHECK(filc_ptr_ptr(ptr), NULL,
               "Fil-C LLVM JIT API: null JIT session.");
    filc_check_access_special(ptr, FILC_SPECIAL_TYPE_LLVM_JIT);
    filc_llvm_jit_session_object* payload = (filc_llvm_jit_session_object*)filc_ptr_ptr(ptr);
    FILC_CHECK(payload->kind == FILC_LLVM_JIT_KIND_JIT_SESSION, NULL,
               "Fil-C LLVM JIT API: expected a JIT session object.");
    return payload;
}

static filc_llvm_jit_executable_module* unwrap_executable_module(filc_thread* my_thread, filc_ptr ptr)
{
    PAS_UNUSED_PARAM(my_thread);
    if (!filc_ptr_ptr(ptr))
        return NULL;
    filc_check_access_special(ptr, FILC_SPECIAL_TYPE_LLVM_JIT);
    filc_llvm_jit_executable_module* payload =
        (filc_llvm_jit_executable_module*)filc_ptr_ptr(ptr);
    FILC_CHECK(payload->kind == FILC_LLVM_JIT_KIND_EXECUTABLE_MODULE, NULL,
               "Fil-C LLVM JIT API: expected a JIT executable module object.");
    return payload;
}

static void* handle_of(filc_llvm_jit_object* payload)
{
    return payload ? payload->handle : NULL;
}

static void** decode_handle_array(filc_thread* my_thread, filc_ptr array_ptr, size_t count,
                                  filc_llvm_jit_kind expected,
                                  filc_llvm_jit_context_object** context_inout,
                                  filc_llvm_jit_object** module_inout)
{
    size_t index;
    void** result;
    if (!filc_ptr_ptr(array_ptr)) {
        FILC_CHECK(!count, NULL,
                   "Fil-C LLVM JIT API: null array with nonzero count.");
        return NULL;
    }
    result = (void**)filc_bmalloc_allocate_tmp(
        my_thread, filc_mul_size(count, sizeof(void*)));
    for (index = 0; index < count; ++index) {
        filc_ptr element = filc_load_ptr(my_thread, array_ptr,
                                         (ptrdiff_t)filc_mul_size(index, sizeof(void*)));
        filc_llvm_jit_object* payload = unwrap_obj_or_null(
            my_thread, element, expected, context_inout, module_inout);
        result[index] = handle_of(payload);
    }
    return result;
}

void filc_llvm_jit_destruct(void* raw_payload)
{
    filc_llvm_jit_kind kind = *(filc_llvm_jit_kind*)raw_payload;
    switch (kind) {
    case FILC_LLVM_JIT_KIND_CONTEXT: {
        filc_llvm_jit_context_object* object = (filc_llvm_jit_context_object*)raw_payload;
        drop_context_state(object);
        return;
    }
    case FILC_LLVM_JIT_KIND_MODULE: {
        filc_llvm_jit_module_object* object = (filc_llvm_jit_module_object*)raw_payload;
        api->LLVMDisposeModule(object->base.handle);
        drop_context_state(object->base.context);
        return;
    }
    case FILC_LLVM_JIT_KIND_BUILDER: {
        filc_llvm_jit_builder_object* object = (filc_llvm_jit_builder_object*)raw_payload;
        api->LLVMDisposeBuilder(object->base.handle);
        drop_context_state(object->base.context);
        return;
    }
    case FILC_LLVM_JIT_KIND_OPERAND_BUNDLE: {
        filc_llvm_jit_object* object = (filc_llvm_jit_object*)raw_payload;
        api->LLVMDisposeOperandBundle(object->handle);
        return;
    }
    case FILC_LLVM_JIT_KIND_METADATA_ENTRIES: {
        filc_llvm_jit_entries_object* object = (filc_llvm_jit_entries_object*)raw_payload;
        api->LLVMDisposeValueMetadataEntries(object->base.handle);
        return;
    }
    case FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES: {
        filc_llvm_jit_entries_object* object = (filc_llvm_jit_entries_object*)raw_payload;
        api->LLVMDisposeModuleFlagsMetadata(object->base.handle);
        return;
    }
    case FILC_LLVM_JIT_KIND_JIT_SESSION: {
        filc_llvm_jit_session_object* object =
            (filc_llvm_jit_session_object*)raw_payload;
        api->ShimJITDestroy((ShimJIT*)object->handle);
        filc_object_array_destruct(&object->objects);
        return;
    }
    case FILC_LLVM_JIT_KIND_EXECUTABLE_MODULE: {
        filc_llvm_jit_executable_module* object =
            (filc_llvm_jit_executable_module*)raw_payload;
        size_t index;
        for (index = object->jit_bases.size; index--;)
            jit_heap_deallocate(object->jit_bases.array[index]);
        filc_raw_ptr_array_destruct(&object->jit_bases);
        filc_object_array_destruct(&object->objects);
        for (index = object->sections.size; index--;)
            bmalloc_deallocate(object->sections.array[index]);
        filc_raw_ptr_array_destruct(&object->sections);
        for (index = object->auxes.size; index--;)
            bmalloc_deallocate(object->auxes.array[index]);
        filc_raw_ptr_array_destruct(&object->auxes);
        api->ShimExportMappingDestroy(object->mapping);
        return;
    }
    default:
        PAS_ASSERT(!"Bad kind");
        return;
    }
}

static void code_fence(filc_thread* my_thread, void* arg)
{
    PAS_UNUSED_PARAM(my_thread);
    PAS_UNUSED_PARAM(arg);
#if defined(__x86_64__)
    unsigned a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0) : "memory");
#elif defined(__aarch64__)
    __asm__ volatile("isb" : : : "memory");
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}

filc_ptr pizlonated___atomic_load(filc_thread* my_thread, const filc_origin* passed_origin);
filc_ptr pizlonated___atomic_store(filc_thread* my_thread, const filc_origin* passed_origin);
filc_ptr pizlonated___atomic_compare_exchange(filc_thread* my_thread,
                                              const filc_origin* passed_origin);

static shim_ptr to_shim_ptr(filc_ptr ptr)
{
    shim_ptr result;
    result.ptr = ptr.ptr;
    result.lower = ptr.lower;
    return result;
}

filc_ptr filc_native_FilLLVMJIT_Create(filc_thread* my_thread)
{
    ensure_api(my_thread);
    ShimJIT* handle = api->ShimJITCreate();
    PAS_ASSERT(handle);
    filc_object* object = filc_allocate_special_with_heap(
        my_thread, sizeof(filc_llvm_jit_session_object), 1,
        FILC_SPECIAL_TYPE_LLVM_JIT, fugc_destructor_heap);
    filc_llvm_jit_session_object* session =
        (filc_llvm_jit_session_object*)filc_object_lower(object);
    session->kind = FILC_LLVM_JIT_KIND_JIT_SESSION;
    pas_lock_construct(&session->lock);
    session->handle = (void*)handle;
    filc_object_array_construct(&session->objects);
    api->ShimJITSetGlobalMapping(
        session->handle, "__atomic_load", to_shim_ptr(pizlonated___atomic_load(my_thread, NULL)));
    api->ShimJITSetGlobalMapping(
        session->handle, "__atomic_store", to_shim_ptr(pizlonated___atomic_store(my_thread, NULL)));
    api->ShimJITSetGlobalMapping(
        session->handle, "__atomic_compare_exchange",
        to_shim_ptr(pizlonated___atomic_compare_exchange(my_thread, NULL)));
    pas_store_store_fence();
    return filc_ptr_create_with_special_object(my_thread, object);
}

void filc_native_FilLLVMJIT_SetOption(filc_thread* my_thread, filc_ptr jit_ptr,
                                      filc_ptr key_ptr, filc_ptr value_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_session_object* session = unwrap_session(my_thread, jit_ptr);
    char* key = filc_check_and_get_tmp_str(my_thread, key_ptr);
    char* value = filc_check_and_get_tmp_str(my_thread, value_ptr);
    pas_lock_lock(&session->lock);
    api->ShimJITSetOption(session->handle, key, value);
    pas_lock_unlock(&session->lock);
}

void filc_native_FilLLVMJIT_SetGlobalMapping(filc_thread* my_thread, filc_ptr jit_ptr,
                                             filc_ptr key_ptr, filc_ptr destination_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_session_object* session = unwrap_session(my_thread, jit_ptr);
    char* key = filc_check_and_get_tmp_str(my_thread, key_ptr);
    pas_lock_lock(&session->lock);
    filc_store_barrier(my_thread, filc_ptr_object(destination_ptr));
    filc_object_array_push(&session->objects, filc_ptr_object(destination_ptr));
    if (verbose) {
        pas_log("Setting mapping of %s to ", key);
        filc_ptr_dump(destination_ptr, pas_log_stream);
        pas_log("\n");
    }
    api->ShimJITSetGlobalMapping(session->handle, key, to_shim_ptr(destination_ptr));
    pas_lock_unlock(&session->lock);
}

void filc_native_FilLLVMJIT_BlessPointer(filc_thread* my_thread, filc_ptr jit_ptr,
                                         filc_ptr destination_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_session_object* session = unwrap_session(my_thread, jit_ptr);
    pas_lock_lock(&session->lock);
    filc_store_barrier(my_thread, filc_ptr_object(destination_ptr));
    filc_object_array_push(&session->objects, filc_ptr_object(destination_ptr));
    api->ShimJITSetPointerMapping(session->handle, to_shim_ptr(destination_ptr));
    pas_lock_unlock(&session->lock);
}

filc_ptr filc_native_FilLLVMJIT_GetPointerToGlobal(filc_thread* my_thread, filc_ptr module_ptr,
                                                   filc_ptr name_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_executable_module* module = unwrap_executable_module(my_thread, module_ptr);
    char* name = filc_check_and_get_tmp_str(my_thread, name_ptr);
    pas_lock_lock(&module->lock);
    pizlonated_getter getter = api->ShimExportMappingLookup(module->mapping, name);
    pas_lock_unlock(&module->lock);
    if (verbose)
        pas_log("For %s got getter %p\n", name, getter);
    if (!getter)
        return filc_ptr_forge_null();
    filc_ptr result = getter(my_thread, NULL);
    if (verbose) {
        pas_log("Returning ");
        filc_ptr_dump(result, pas_log_stream);
        pas_log("\n");
    }
    return result;
}

filc_ptr filc_native_FilLLVMJIT_CompileModule(filc_thread* my_thread, filc_ptr jit_ptr,
                                              filc_ptr module_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_session_object* session = unwrap_session(my_thread, jit_ptr);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module_obj =
        unwrap_obj(my_thread, module_ptr, FILC_LLVM_JIT_KIND_MODULE,
                   &context, NULL);
    PAS_ASSERT(context);
    /* FIXME: Would be cool if we exited around the compile. */
    pas_lock_lock(&session->lock);
    pas_lock_lock(&context->lock);
    filc_llvm_jit_executable_module* module = api->ShimJITCompile(
        my_thread, session, session->handle, module_obj->handle);
    pas_lock_unlock(&context->lock);
    pas_lock_unlock(&session->lock);
    filc_exit(my_thread);
    filc_soft_handshake(code_fence, NULL);
    filc_enter(my_thread);
    return filc_ptr_for_special_payload_with_manual_tracking(module);
}

static void* shim_allocate_executable_module(void* raw_my_thread, void* raw_jit_session)
{
    filc_thread* my_thread = raw_my_thread;
    filc_llvm_jit_executable_module* module = filc_object_special_payload(
        my_thread, filc_allocate_special_with_heap(
            my_thread, sizeof(filc_llvm_jit_executable_module), 1,
            FILC_SPECIAL_TYPE_LLVM_JIT, fugc_destructor_heap));
    module->kind = FILC_LLVM_JIT_KIND_EXECUTABLE_MODULE;
    pas_lock_construct(&module->lock);
    filc_raw_ptr_array_construct(&module->jit_bases);
    filc_object_array_construct(&module->objects);
    filc_raw_ptr_array_construct(&module->sections);
    filc_raw_ptr_array_construct(&module->auxes);
    module->mapping = NULL;

    filc_llvm_jit_session_object* session = (filc_llvm_jit_session_object*)raw_jit_session;
    PAS_ASSERT(session->kind == FILC_LLVM_JIT_KIND_JIT_SESSION);
    size_t index;
    for (index = filc_object_array_num_objects(&session->objects); index--;) {
        filc_store_barrier(my_thread, filc_object_array_at(&session->objects, index));
        filc_object_array_push(&module->objects, filc_object_array_at(&session->objects, index));
    }

    if (verbose)
        pas_log("Allocated module = %p\n", module);
    return module;
}

static void* shim_executable_module_allocate_object(void* raw_my_thread, void* raw_module,
                                                    size_t size, size_t alignment)
{
    filc_thread* my_thread = raw_my_thread;
    filc_llvm_jit_executable_module* module = raw_module;
    alignment = filc_finalizable_alignment(alignment);
    size_t offset_to_payload;
    size_t total_size;
    filc_prepare_allocate(&size, alignment, &offset_to_payload, &total_size);
    void* result = (void*)
        verse_heap_allocate_with_alignment(fugc_destructor_heap, total_size, alignment).begin;
    memset(result, 0, total_size);
    filc_alignment_header_construct(my_thread, result, alignment, NULL);
    void* lower = (char*)result + offset_to_payload;
    filc_object* object = filc_object_for_lower(lower);
    filc_store_barrier(my_thread, object);
    filc_raw_ptr_array_add(&module->object_bases, result);
    if (verbose)
        pas_log("Allocated object = %p\n", object);
    return object;
}

static void* shim_executable_module_allocate_data(void* raw_my_thread, void* raw_module,
                                                  size_t size, size_t alignment)
{
    PAS_UNUSED_PARAM(raw_my_thread);
    filc_llvm_jit_executable_module* module = raw_module;
    void* result = bmalloc_allocate_zeroed_with_alignment(size, alignment);
    filc_raw_ptr_array_add(&module->sections, result);
    if (verbose)
        pas_log("Allocated data = %p\n", result);
    return result;
}

static void* shim_executable_module_allocate_executable(void* raw_my_thread, void* raw_module,
                                                        size_t size)
{
    PAS_UNUSED_PARAM(raw_my_thread);
    filc_llvm_jit_executable_module* module = raw_module;
    void* result = jit_heap_try_allocate(size);
    PAS_ASSERT(result);
    filc_raw_ptr_array_add(&module->jit_bases, result);
    if (verbose)
        pas_log("Allocated executable = %p\n", result);
    return result;
}

static void* shim_executable_module_allocate_aux(void* raw_my_thread, void* raw_module, size_t size)
{
    PAS_UNUSED_PARAM(raw_my_thread);
    filc_llvm_jit_executable_module* module = raw_module;
    filc_llvm_jit_aux_allocation* result =
        bmalloc_allocate_zeroed(PAS_OFFSETOF(filc_llvm_jit_aux_allocation, array) + size);
    filc_raw_ptr_array_add(&module->auxes, result);
    result->size = size;
    if (verbose)
        pas_log("Allocated aux = %p\n", &result->array);
    return &result->array;
}

static void shim_executable_module_finalize_objects(void* raw_my_thread, void* raw_module)
{
    PAS_UNUSED_PARAM(raw_my_thread);
    filc_llvm_jit_executable_module* module = raw_module;
    size_t index;
    if (verbose)
        pas_log("Finalizing objects:\n");
    for (index = module->object_bases.size; index--;) {
        filc_alignment_header* header = module->object_bases.array[index];
        filc_object* object = filc_alignment_header_get_object(header);
        if (verbose) {
            pas_log("    before: ");
            filc_object_dump(object, pas_log_stream);
            pas_log("\n");
        }
        filc_object_flags flags = filc_aux_get_flags(object->aux);
        PAS_ASSERT(!(flags & FILC_OBJECT_FLAG_GLOBAL));
        object->aux = filc_aux_create(
            filc_object_flags_create(
                flags & FILC_OBJECT_FLAGS_MASK,
                filc_object_flags_special_type(flags),
                pas_log2(filc_alignment_header_get_alignment(header))),
            filc_aux_get_ptr(object->aux));
        if (verbose) {
            pas_log("    after: ");
            filc_object_dump(object, pas_log_stream);
            pas_log("\n");
        }
    }

#if PAS_ARM64
    uintptr_t ctr;
    
    asm volatile("mrs %0, ctr_el0" : "=r"(ctr));

    for (index = module->jit_bases.size; index--;) {
        uintptr_t start = (uintptr_t)module->jit_bases[index];
        size_t size = jit_heap_get_size((void*)start);
        uintptr_t end = start + size;
        
        /* IDC (bit 28): D-cache clean to PoU not required for I/D coherence. */
        if (!(ctr & (1ull << 28))) {
            uintptr_t dline = 4ull << ((ctr >> 16) & 0xf);   /* DminLine: log2 words */
            for (uintptr_t p = start & ~(dline - 1); p < end; p += dline)
                asm volatile("dc cvau, %0" :: "r"(p) : "memory");
        }
        asm volatile("dsb ish" ::: "memory");   /* stores/cleans visible before IC */
        
        /* DIC (bit 29): I-cache invalidation to PoU not required. */
        if (!(ctr & (1ull << 29))) {
            uintptr_t iline = 4ull << (ctr & 0xf);           /* IminLine: log2 words */
            for (uintptr_t p = start & ~(iline - 1); p < end; p += iline)
                asm volatile("ic ivau, %0" :: "r"(p) : "memory");
            asm volatile("dsb ish" ::: "memory");
        }
    }
#endif
}

static void shim_executable_module_set_export_mapping(void* raw_my_thread, void* raw_module,
                                                      ShimExportMapping* mapping)
{
    PAS_UNUSED_PARAM(raw_my_thread);
    filc_llvm_jit_executable_module* module = raw_module;
    module->mapping = mapping;
}

/* Hand-written natives for functions that the mechanical generator
   cannot express: out-arrays (with companion count queries), out-scalars,
   raw-word arrays, and multi-array calls. */

void filc_native_LLVMGetVersion(filc_thread* my_thread, filc_ptr major_ptr,
                                      filc_ptr minor_ptr, filc_ptr patch_ptr)
{
    ensure_api(my_thread);
    unsigned major = 0;
    unsigned minor = 0;
    unsigned patch = 0;
    filc_check_write(major_ptr, sizeof(unsigned));
    filc_check_write(minor_ptr, sizeof(unsigned));
    filc_check_write(patch_ptr, sizeof(unsigned));
    api->LLVMGetVersion(&major, &minor, &patch);
    *(unsigned*)filc_ptr_ptr(major_ptr) = major;
    *(unsigned*)filc_ptr_ptr(minor_ptr) = minor;
    *(unsigned*)filc_ptr_ptr(patch_ptr) = patch;
}

double filc_native_LLVMConstRealGetDouble(filc_thread* my_thread, filc_ptr value_ptr,
                                                filc_ptr loses_info_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* value =
        unwrap_obj(my_thread, value_ptr, FILC_LLVM_JIT_KIND_VALUE,
                   &context, &module);
    int loses_info = 0;
    double result;
    filc_check_write(loses_info_ptr, sizeof(int));
    pas_lock_lock(&context->lock);
    result = api->LLVMConstRealGetDouble((LLVMValueRef)value->handle, &loses_info);
    pas_lock_unlock(&context->lock);
    *(int*)filc_ptr_ptr(loses_info_ptr) = loses_info;
    return result;
}

filc_ptr filc_native_LLVMGlobalCopyAllMetadata(filc_thread* my_thread, filc_ptr value_ptr,
                                               filc_ptr num_entries_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* value =
        unwrap_obj(my_thread, value_ptr, FILC_LLVM_JIT_KIND_VALUE,
                                 &context, &module);
    size_t num_entries = 0;
    LLVMValueMetadataEntry* entries = NULL;
    filc_check_write(num_entries_ptr, sizeof(size_t));
    pas_lock_lock(&context->lock);
    entries = api->LLVMGlobalCopyAllMetadata((LLVMValueRef)value->handle, &num_entries);
    /* The entries array is never freed (the curated API has no Dispose for it); the
       wrapper records the count so the getters can bounds-check. */
    filc_ptr result = wrap_handle(my_thread, context, module,
                                  FILC_LLVM_JIT_KIND_METADATA_ENTRIES,
                                  (void*)entries);
    if (filc_ptr_ptr(result)) {
        ((filc_llvm_jit_entries_object*)filc_ptr_ptr(result))->count = num_entries;
    }
    pas_lock_unlock(&context->lock);
    *(size_t*)filc_ptr_ptr(num_entries_ptr) = num_entries;
    return result;
}

filc_ptr filc_native_LLVMModuleFlagEntriesGetKey(filc_thread* my_thread,
                                                 filc_ptr entries_ptr, unsigned index)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* entries =
        unwrap_obj(my_thread, entries_ptr,
                   FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES,
                   &context, &module);
    FILC_CHECK(context, NULL, "LLVMModuleFlagEntriesGetKey: entries without context.");
    const char* key;
    pas_lock_lock(&context->lock);
    /* Len is redundant: Fil-C strings know their own length. */
    key = api->LLVMModuleFlagEntriesGetKey((LLVMModuleFlagEntry*)entries->handle, index,
                                           NULL);
    filc_ptr result = key ? filc_strdup(my_thread, key) : filc_ptr_forge_null();
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMTargetExtTypeInContext(
    filc_thread* my_thread, filc_ptr context_ptr, filc_ptr name_ptr,
    filc_ptr type_params_ptr, unsigned type_param_count,
    filc_ptr int_params_ptr, unsigned int_param_count)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = unwrap_context(my_thread, context_ptr);
    filc_llvm_jit_object* module = NULL;
    char* name = filc_check_and_get_tmp_str(my_thread, name_ptr);
    LLVMTypeRef* type_params = NULL;
    unsigned* int_params = NULL;
    if (filc_ptr_ptr(type_params_ptr)) {
        void** decoded = decode_handle_array(
            my_thread, type_params_ptr, type_param_count, FILC_LLVM_JIT_KIND_TYPE,
            &context, &module);
        type_params = (LLVMTypeRef*)decoded;
    } else {
        FILC_CHECK(!type_param_count, NULL,
                   "LLVMTargetExtTypeInContext: null type params with nonzero count.");
    }
    if (filc_ptr_ptr(int_params_ptr)) {
        filc_check_read(int_params_ptr, filc_mul_size(int_param_count, sizeof(unsigned)));
        int_params = (unsigned*)filc_ptr_ptr(int_params_ptr);
    } else {
        FILC_CHECK(!int_param_count, NULL,
                   "LLVMTargetExtTypeInContext: null int params with nonzero count.");
    }
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMTargetExtTypeInContext((LLVMContextRef)context->handle, name,
                                                 type_params, type_param_count,
                                                 int_params, int_param_count);
    filc_ptr result = wrap_handle(my_thread, context, NULL,
                                         FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstIntOfArbitraryPrecision(
    filc_thread* my_thread, filc_ptr int_ty_ptr, unsigned num_words, filc_ptr words_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* int_ty =
        unwrap_obj(my_thread, int_ty_ptr, FILC_LLVM_JIT_KIND_TYPE,
                                 &context, &module);
    const uint64_t* words = NULL;
    if (filc_ptr_ptr(words_ptr)) {
        filc_check_read(words_ptr, filc_mul_size(num_words, sizeof(uint64_t)));
        words = (const uint64_t*)filc_ptr_ptr(words_ptr);
    } else {
        FILC_CHECK(!num_words, NULL,
                   "LLVMConstIntOfArbitraryPrecision: null words with nonzero count.");
    }
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstIntOfArbitraryPrecision((LLVMTypeRef)int_ty->handle, num_words,
                                                       words);
    filc_ptr result = wrap_handle(my_thread, context, module,
                                         FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateConstantRangeAttribute(
    filc_thread* my_thread, filc_ptr context_ptr, unsigned kind_id, unsigned num_bits,
    filc_ptr lower_words_ptr, filc_ptr upper_words_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = unwrap_context(my_thread, context_ptr);
    /* The C API derives the word count from the bit width. */
    unsigned num_words = (num_bits + 63u) / 64u;
    const uint64_t* lower_words = NULL;
    const uint64_t* upper_words = NULL;
    if (filc_ptr_ptr(lower_words_ptr)) {
        filc_check_read(lower_words_ptr, filc_mul_size(num_words, sizeof(uint64_t)));
        lower_words = (const uint64_t*)filc_ptr_ptr(lower_words_ptr);
    } else {
        FILC_CHECK(!num_words, NULL,
                   "LLVMCreateConstantRangeAttribute: null lower words with nonzero count.");
    }
    if (filc_ptr_ptr(upper_words_ptr)) {
        filc_check_read(upper_words_ptr, filc_mul_size(num_words, sizeof(uint64_t)));
        upper_words = (const uint64_t*)filc_ptr_ptr(upper_words_ptr);
    } else {
        FILC_CHECK(!num_words, NULL,
                   "LLVMCreateConstantRangeAttribute: null upper words with nonzero count.");
    }
    LLVMAttributeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMCreateConstantRangeAttribute((LLVMContextRef)context->handle,
                                                       kind_id, num_bits,
                                                       lower_words, upper_words);
    filc_ptr result = wrap_handle(my_thread, context, NULL,
                                         FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCallBr(
    filc_thread* my_thread, filc_ptr builder_ptr, filc_ptr ty_ptr, filc_ptr fn_ptr,
    filc_ptr default_dest_ptr, filc_ptr indirect_dests_ptr, unsigned num_indirect_dests,
    filc_ptr args_ptr, unsigned num_args, filc_ptr bundles_ptr, unsigned num_bundles,
    filc_ptr name_ptr)
{
    ensure_api(my_thread);
    /* Operand bundles have no constructors in the curated API, so only null is
       accepted today (the count must be zero with it). */
    FILC_CHECK(!filc_ptr_ptr(bundles_ptr), NULL,
               "LLVMBuildCallBr: operand bundles are not supported in the curated API (pass null).");
    FILC_CHECK(!num_bundles, NULL,
               "LLVMBuildCallBr: nonzero bundle count with null bundles.");
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* builder =
        unwrap_obj(my_thread, builder_ptr, FILC_LLVM_JIT_KIND_BUILDER,
                   &context, &module);
    filc_llvm_jit_object* ty =
        unwrap_obj(my_thread, ty_ptr, FILC_LLVM_JIT_KIND_TYPE,
                   &context, &module);
    filc_llvm_jit_object* fn =
        unwrap_obj(my_thread, fn_ptr, FILC_LLVM_JIT_KIND_VALUE,
                   &context, &module);
    filc_llvm_jit_object* default_dest =
        unwrap_obj(my_thread, default_dest_ptr, FILC_LLVM_JIT_KIND_BASIC_BLOCK,
                   &context, &module);
    LLVMBasicBlockRef* indirect_dests = (LLVMBasicBlockRef*)decode_handle_array(
        my_thread, indirect_dests_ptr, num_indirect_dests, FILC_LLVM_JIT_KIND_BASIC_BLOCK,
        &context, &module);
    LLVMValueRef* args = (LLVMValueRef*)decode_handle_array(
        my_thread, args_ptr, num_args, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* name = filc_check_and_get_tmp_str(my_thread, name_ptr);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildCallBr((LLVMBuilderRef)builder->handle,
                                      (LLVMTypeRef)handle_of(ty),
                                      (LLVMValueRef)handle_of(fn),
                                      (LLVMBasicBlockRef)handle_of(default_dest),
                                      indirect_dests, num_indirect_dests,
                                      args, num_args, NULL, 0, name);
    filc_ptr result = wrap_handle(my_thread, context, module,
                                         FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (module)
        builder_adopt_module(my_thread, builder, module);
    pas_lock_unlock(&context->lock);
    return result;
}

/* Out-array pattern shared by the LLVMGet* family: the caller queries the
   companion count and the getter into a host tmp buffer (under the context
   lock, exited), then this emits wrappers into the caller's preallocated
   Fil-C pointer array with checked stores (entered). Under-allocation
   panics cleanly instead of corrupting memory. */
static void emit_out_array(filc_thread* my_thread,
                                         filc_llvm_jit_context_object* context,
                                         filc_llvm_jit_object* module,
                                         void** raws, unsigned count,
                                         filc_ptr array_ptr, filc_llvm_jit_kind kind)
{
    unsigned index;
    for (index = 0; index < count; ++index) {
        filc_ptr wrapped = wrap_handle(my_thread, context, module, kind,
                                              raws[index]);
        filc_store_ptr(my_thread, array_ptr,
                       (ptrdiff_t)filc_mul_size(index, sizeof(void*)), wrapped);
    }
}


void filc_native_LLVMGetAttributesAtIndex(filc_thread* my_thread, filc_ptr fn_ptr,
                                                unsigned index, filc_ptr attrs_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* fn = unwrap_obj(
        my_thread, fn_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMAttributeRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    count = api->LLVMGetAttributeCountAtIndex((LLVMValueRef)fn->handle, index);
    if (count)
        tmp = (LLVMAttributeRef*)malloc((size_t)count * sizeof(LLVMAttributeRef));
    if (!count || tmp)
        api->LLVMGetAttributesAtIndex((LLVMValueRef)fn->handle, index, tmp);
    if (count && !tmp) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetAttributesAtIndex.");
    }
    emit_out_array(my_thread, context, module, (void**)tmp, count, attrs_ptr,
                                 FILC_LLVM_JIT_KIND_ATTRIBUTE);
    free(tmp);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMGetCallSiteAttributes(filc_thread* my_thread, filc_ptr call_ptr,
                                                 unsigned index, filc_ptr attrs_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* call = unwrap_obj(
        my_thread, call_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMAttributeRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    count = api->LLVMGetCallSiteAttributeCount((LLVMValueRef)call->handle, index);
    if (count)
        tmp = (LLVMAttributeRef*)malloc((size_t)count * sizeof(LLVMAttributeRef));
    if (!count || tmp)
        api->LLVMGetCallSiteAttributes((LLVMValueRef)call->handle, index, tmp);
    if (count && !tmp) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetCallSiteAttributes.");
    }
    emit_out_array(my_thread, context, module, (void**)tmp, count, attrs_ptr,
                                 FILC_LLVM_JIT_KIND_ATTRIBUTE);
    free(tmp);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMGetBasicBlocks(filc_thread* my_thread, filc_ptr fn_ptr,
                                          filc_ptr blocks_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* fn = unwrap_obj(
        my_thread, fn_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    count = api->LLVMCountBasicBlocks((LLVMValueRef)fn->handle);
    if (count)
        tmp = (LLVMBasicBlockRef*)malloc((size_t)count * sizeof(LLVMBasicBlockRef));
    if (!count || tmp)
        api->LLVMGetBasicBlocks((LLVMValueRef)fn->handle, tmp);
    if (count && !tmp) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetBasicBlocks.");
    }
    emit_out_array(my_thread, context, module, (void**)tmp, count, blocks_ptr,
                                 FILC_LLVM_JIT_KIND_BASIC_BLOCK);
    free(tmp);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMGetHandlers(filc_thread* my_thread, filc_ptr catch_switch_ptr,
                                       filc_ptr handlers_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* catch_switch = unwrap_obj(
        my_thread, catch_switch_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    count = api->LLVMGetNumHandlers((LLVMValueRef)catch_switch->handle);
    if (count)
        tmp = (LLVMBasicBlockRef*)malloc((size_t)count * sizeof(LLVMBasicBlockRef));
    if (!count || tmp)
        api->LLVMGetHandlers((LLVMValueRef)catch_switch->handle, tmp);
    if (count && !tmp) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetHandlers.");
    }
    emit_out_array(my_thread, context, module, (void**)tmp, count, handlers_ptr,
                                 FILC_LLVM_JIT_KIND_BASIC_BLOCK);
    free(tmp);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMGetMDNodeOperands(filc_thread* my_thread, filc_ptr value_ptr,
                                             filc_ptr dest_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* value = unwrap_obj(
        my_thread, value_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    count = api->LLVMGetMDNodeNumOperands((LLVMValueRef)value->handle);
    if (count)
        tmp = (LLVMValueRef*)malloc((size_t)count * sizeof(LLVMValueRef));
    if (!count || tmp)
        api->LLVMGetMDNodeOperands((LLVMValueRef)value->handle, tmp);
    if (count && !tmp) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetMDNodeOperands.");
    }
    emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_VALUE);
    free(tmp);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMGetNamedMetadataOperands(filc_thread* my_thread, filc_ptr module_ptr,
                                                    filc_ptr name_ptr, filc_ptr dest_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module_obj = unwrap_obj(
        my_thread, module_ptr, FILC_LLVM_JIT_KIND_MODULE, &context, NULL);
    char* name = filc_check_and_get_tmp_str(my_thread, name_ptr);
    LLVMValueRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    count = api->LLVMGetNamedMetadataNumOperands((LLVMModuleRef)module_obj->handle, name);
    if (count)
        tmp = (LLVMValueRef*)malloc((size_t)count * sizeof(LLVMValueRef));
    if (!count || tmp)
        api->LLVMGetNamedMetadataOperands((LLVMModuleRef)module_obj->handle, name, tmp);
    if (count && !tmp) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetNamedMetadataOperands.");
    }
    emit_out_array(my_thread, context, module_obj, (void**)tmp, count,
                                 dest_ptr, FILC_LLVM_JIT_KIND_VALUE);
    free(tmp);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMGetParamTypes(filc_thread* my_thread, filc_ptr function_ty_ptr,
                                         filc_ptr dest_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* function_ty = unwrap_obj(
        my_thread, function_ty_ptr, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    count = api->LLVMCountParamTypes((LLVMTypeRef)function_ty->handle);
    if (count)
        tmp = (LLVMTypeRef*)malloc((size_t)count * sizeof(LLVMTypeRef));
    if (!count || tmp)
        api->LLVMGetParamTypes((LLVMTypeRef)function_ty->handle, tmp);
    if (count && !tmp) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetParamTypes.");
    }
    emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_TYPE);
    free(tmp);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMGetParams(filc_thread* my_thread, filc_ptr fn_ptr, filc_ptr dest_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* fn = unwrap_obj(
        my_thread, fn_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    count = api->LLVMCountParams((LLVMValueRef)fn->handle);
    if (count)
        tmp = (LLVMValueRef*)malloc((size_t)count * sizeof(LLVMValueRef));
    if (!count || tmp)
        api->LLVMGetParams((LLVMValueRef)fn->handle, tmp);
    if (count && !tmp) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetParams.");
    }
    emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_VALUE);
    free(tmp);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMGetStructElementTypes(filc_thread* my_thread, filc_ptr struct_ty_ptr,
                                                 filc_ptr dest_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* struct_ty = unwrap_obj(
        my_thread, struct_ty_ptr, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    count = api->LLVMCountStructElementTypes((LLVMTypeRef)struct_ty->handle);
    if (count)
        tmp = (LLVMTypeRef*)malloc((size_t)count * sizeof(LLVMTypeRef));
    if (!count || tmp)
        api->LLVMGetStructElementTypes((LLVMTypeRef)struct_ty->handle, tmp);
    if (count && !tmp) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetStructElementTypes.");
    }
    emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_TYPE);
    free(tmp);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMGetSubtypes(filc_thread* my_thread, filc_ptr ty_ptr, filc_ptr dest_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* ty = unwrap_obj(
        my_thread, ty_ptr, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    count = api->LLVMGetNumContainedTypes((LLVMTypeRef)ty->handle);
    if (count)
        tmp = (LLVMTypeRef*)malloc((size_t)count * sizeof(LLVMTypeRef));
    if (!count || tmp)
        api->LLVMGetSubtypes((LLVMTypeRef)ty->handle, tmp);
    if (count && !tmp) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetSubtypes.");
    }
    emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_TYPE);
    free(tmp);
    pas_lock_unlock(&context->lock);
}

filc_ptr filc_native_LLVMContextCreate(filc_thread* my_thread)
{
    ensure_api(my_thread);
    LLVMContextRef handle;
    handle = api->LLVMContextCreate();
    if (!handle)
        return filc_ptr_forge_null();
    filc_object* object = filc_allocate_special_with_heap(
        my_thread, sizeof(filc_llvm_jit_context_object), 1,
        FILC_SPECIAL_TYPE_LLVM_JIT, fugc_destructor_heap);
    filc_llvm_jit_context_object* context =
        (filc_llvm_jit_context_object*)filc_object_lower(object);
    context->kind = FILC_LLVM_JIT_KIND_CONTEXT;
    context->ref_count = 1;
    context->handle = handle;
    pas_lock_construct(&context->lock);
    pas_store_store_fence();
    return filc_ptr_create_with_special_object(my_thread, object);
}

/* ------------------------------------------------------------------ */
/* Borrowed strings and arrays. The C API below returns pointers owned by
   the LLVM objects themselves (no Dispose needed, hence their place
   in the curated header). Wrappers copy the data into fresh Fil-C
   memory while entered; the length out-params of the C API are dropped
   because Fil-C values carry their own bounds. The two
   LLVMIntrinsicCopyOverloadedName* functions are the exception: they return
   freshly allocated strings that the full LLVM C API would release with
   LLVMDisposeMessage, which the curated API omits -- the originals leak by design
   and only the copy is returned. */

static filc_ptr new_sized_string(filc_thread* my_thread, const char* str,
                                              size_t length)
{
    filc_ptr result;
    if (!str)
        return filc_ptr_forge_null();
    result = filc_ptr_create_with_object(my_thread,
                                         filc_allocate(my_thread, length + 1));
    filc_memcpy_with_exit(my_thread, filc_ptr_ptr(result), str, length);
    ((char*)filc_ptr_ptr(result))[length] = '\0';
    return result;
}

#define FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(c_name, kind) \
    filc_ptr filc_native_##c_name(filc_thread* my_thread, filc_ptr parent_ptr) \
    { \
        ensure_api(my_thread); \
        filc_llvm_jit_context_object* context = NULL; \
        filc_llvm_jit_object* module = NULL; \
        filc_llvm_jit_object* parent = unwrap_obj( \
            my_thread, parent_ptr, kind, &context, &module); \
        const char* raw; \
        pas_lock_lock(&context->lock); \
        raw = api->c_name((void*)parent->handle); \
        filc_ptr result = filc_strdup(my_thread, raw); \
        pas_lock_unlock(&context->lock); \
        return result; \
    }

#define FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(c_name, kind, len_type) \
    filc_ptr filc_native_##c_name(filc_thread* my_thread, filc_ptr parent_ptr) \
    { \
        ensure_api(my_thread); \
        filc_llvm_jit_context_object* context = NULL; \
        filc_llvm_jit_object* module = NULL; \
        filc_llvm_jit_object* parent = unwrap_obj( \
            my_thread, parent_ptr, kind, &context, &module); \
        len_type length = 0; \
        const char* raw; \
        pas_lock_lock(&context->lock); \
        raw = api->c_name((void*)parent->handle, &length); \
        filc_ptr result = \
            new_sized_string(my_thread, raw, (size_t)length); \
        pas_lock_unlock(&context->lock); \
        return result; \
    }

FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetBasicBlockName, FILC_LLVM_JIT_KIND_BASIC_BLOCK)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetDataLayout, FILC_LLVM_JIT_KIND_MODULE)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetDataLayoutStr, FILC_LLVM_JIT_KIND_MODULE)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetGC, FILC_LLVM_JIT_KIND_VALUE)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetSection, FILC_LLVM_JIT_KIND_VALUE)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetStructName, FILC_LLVM_JIT_KIND_TYPE)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetTarget, FILC_LLVM_JIT_KIND_MODULE)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetTargetExtTypeName, FILC_LLVM_JIT_KIND_TYPE)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetValueName, FILC_LLVM_JIT_KIND_VALUE)

FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetDebugLocDirectory, FILC_LLVM_JIT_KIND_VALUE,
                                       unsigned)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetDebugLocFilename, FILC_LLVM_JIT_KIND_VALUE,
                                       unsigned)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetInlineAsmAsmString, FILC_LLVM_JIT_KIND_VALUE,
                                       size_t)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetInlineAsmConstraintString,
                                       FILC_LLVM_JIT_KIND_VALUE, size_t)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetMDString, FILC_LLVM_JIT_KIND_VALUE, unsigned)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetModuleIdentifier, FILC_LLVM_JIT_KIND_MODULE,
                                       size_t)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetModuleInlineAsm, FILC_LLVM_JIT_KIND_MODULE,
                                       size_t)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetNamedMetadataName,
                                       FILC_LLVM_JIT_KIND_NAMED_METADATA, size_t)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetSourceFileName, FILC_LLVM_JIT_KIND_MODULE,
                                       size_t)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetStringAttributeKind,
                                       FILC_LLVM_JIT_KIND_ATTRIBUTE, unsigned)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetStringAttributeValue,
                                       FILC_LLVM_JIT_KIND_ATTRIBUTE, unsigned)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetValueName2, FILC_LLVM_JIT_KIND_VALUE, size_t)

filc_ptr filc_native_LLVMIntrinsicGetName(filc_thread* my_thread, unsigned id)
{
    ensure_api(my_thread);
    size_t length = 0;
    const char* raw;
    raw = api->LLVMIntrinsicGetName(id, &length);
    return new_sized_string(my_thread, raw, length);
}

filc_ptr filc_native_LLVMGetIndices(filc_thread* my_thread, filc_ptr inst_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* inst = unwrap_obj(
        my_thread, inst_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned count = 0;
    const unsigned* indices = NULL;
    pas_lock_lock(&context->lock);
    count = api->LLVMGetNumIndices((LLVMValueRef)inst->handle);
    indices = api->LLVMGetIndices((LLVMValueRef)inst->handle);
    filc_ptr result = filc_ptr_create_with_object(
        my_thread, filc_allocate(my_thread, filc_mul_size(count, sizeof(unsigned))));
    if (count)
        filc_memcpy_with_exit(my_thread, filc_ptr_ptr(result), indices,
                              filc_mul_size(count, sizeof(unsigned)));
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMInstructionGetAllMetadataOtherThanDebugLoc(
    filc_thread* my_thread, filc_ptr inst_ptr, filc_ptr num_entries_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* inst = unwrap_obj(
        my_thread, inst_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    size_t num_entries = 0;
    LLVMValueMetadataEntry* entries = NULL;
    filc_check_write(num_entries_ptr, sizeof(size_t));
    pas_lock_lock(&context->lock);
    entries = api->LLVMInstructionGetAllMetadataOtherThanDebugLoc((LLVMValueRef)inst->handle,
                                                                  &num_entries);
    filc_ptr result = wrap_handle(my_thread, context, module,
                                         FILC_LLVM_JIT_KIND_METADATA_ENTRIES,
                                         (void*)entries);
    if (filc_ptr_ptr(result)) {
        ((filc_llvm_jit_entries_object*)filc_ptr_ptr(result))->count = num_entries;
    }
    pas_lock_unlock(&context->lock);
    *(size_t*)filc_ptr_ptr(num_entries_ptr) = num_entries;
    return result;
}

filc_ptr filc_native_LLVMIntrinsicCopyOverloadedName(
    filc_thread* my_thread, unsigned id, filc_ptr param_types_ptr, size_t param_count)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    LLVMTypeRef* param_types = (LLVMTypeRef*)decode_handle_array(
        my_thread, param_types_ptr, param_count, FILC_LLVM_JIT_KIND_TYPE,
        &context, &module);
    size_t name_length = 0;
    char* raw;
    pas_lock_lock(&context->lock);
    /* Owned by LLVM (would need LLVMDisposeMessage in the full C API, which
       the curated API omits): the original leaks by design, only the copy escapes. */
    raw = api->LLVMIntrinsicCopyOverloadedName(id, param_types, param_count, &name_length);
    filc_ptr result = new_sized_string(my_thread, raw, name_length);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIntrinsicCopyOverloadedName2(
    filc_thread* my_thread, filc_ptr module_ptr, unsigned id,
    filc_ptr param_types_ptr, size_t param_count)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module_obj = unwrap_obj(
        my_thread, module_ptr, FILC_LLVM_JIT_KIND_MODULE, &context, NULL);
    LLVMTypeRef* param_types = (LLVMTypeRef*)decode_handle_array(
        my_thread, param_types_ptr, param_count, FILC_LLVM_JIT_KIND_TYPE,
        &context, &module_obj);
    size_t name_length = 0;
    char* raw;
    pas_lock_lock(&context->lock);
    raw = api->LLVMIntrinsicCopyOverloadedName2((LLVMModuleRef)module_obj->handle, id,
                                                param_types, param_count, &name_length);
    filc_ptr result = new_sized_string(my_thread, raw, name_length);
    pas_lock_unlock(&context->lock);
    return result;
}

/* ------------------------------------------------------------------ */
/* Mechanically generated thin wrappers for the curated LLVM C API.
   Thin wrappers mechanically emitted for the curated LLVM C API;
   do not edit by hand. */
filc_ptr filc_native_LLVMAddAlias2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMAddAlias2((LLVMModuleRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), p2, (LLVMValueRef)handle_of(a3), a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMAddAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMAddAttributeAtIndex((LLVMValueRef)handle_of(a0), p1, (LLVMAttributeRef)handle_of(a2));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMAddCallSiteAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMAddCallSiteAttribute((LLVMValueRef)handle_of(a0), p1, (LLVMAttributeRef)handle_of(a2));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMAddCase(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMAddCase((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMBasicBlockRef)handle_of(a2));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMAddClause(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMAddClause((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMAddDestination(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMAddDestination((LLVMValueRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

filc_ptr filc_native_LLVMAddFunction(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMAddFunction((LLVMModuleRef)handle_of(a0), a1, (LLVMTypeRef)handle_of(a2));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMAddGlobal(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMAddGlobal((LLVMModuleRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMAddGlobalIFunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMAddGlobalIFunc: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a5 = unwrap_obj(my_thread, p5, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMAddGlobalIFunc((LLVMModuleRef)handle_of(a0), a1, p2, (LLVMTypeRef)handle_of(a3), p4, (LLVMValueRef)handle_of(a5));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMAddGlobalInAddressSpace(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMAddGlobalInAddressSpace((LLVMModuleRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), a2, p3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMAddHandler(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMAddHandler((LLVMValueRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMAddIncoming(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)decode_handle_array(my_thread, p1, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef* a2 = (LLVMBasicBlockRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMAddIncoming((LLVMValueRef)handle_of(a0), a1, a2, p3);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMAddMetadataToInst(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMAddMetadataToInst((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMAddModuleFlag(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, size_t p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a2;
    if (!filc_ptr_ptr(p2)) {
        FILC_CHECK(p3 == 0, NULL, "LLVMAddModuleFlag: null string with nonzero length.");
        a2 = NULL;
    } else {
        filc_check_read(p2, (size_t)p3);
        a2 = (const char*)filc_ptr_ptr(p2);
    }
    filc_llvm_jit_object* a4 = unwrap_obj(my_thread, p4, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMAddModuleFlag((LLVMModuleRef)handle_of(a0), p1, a2, p3, (LLVMMetadataRef)handle_of(a4));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMAddNamedMetadataOperand(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMAddNamedMetadataOperand((LLVMModuleRef)handle_of(a0), a1, (LLVMValueRef)handle_of(a2));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMAddTargetDependentFunctionAttr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    pas_lock_lock(&context->lock);
    api->LLVMAddTargetDependentFunctionAttr((LLVMValueRef)handle_of(a0), a1, a2);
    pas_lock_unlock(&context->lock);
}

filc_ptr filc_native_LLVMAliasGetAliasee(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMAliasGetAliasee((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMAliasSetAliasee(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMAliasSetAliasee((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

filc_ptr filc_native_LLVMAlignOf(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMAlignOf((LLVMTypeRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMAppendBasicBlockInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMAppendBasicBlockInContext((LLVMContextRef)a0->handle, (LLVMValueRef)handle_of(a1), a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMAppendExistingBasicBlock(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMAppendExistingBasicBlock((LLVMValueRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMAppendModuleInlineAsm(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMAppendModuleInlineAsm: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    pas_lock_lock(&context->lock);
    api->LLVMAppendModuleInlineAsm((LLVMModuleRef)handle_of(a0), a1, p2);
    pas_lock_unlock(&context->lock);
}

filc_ptr filc_native_LLVMArrayType(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMArrayType((LLVMTypeRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMArrayType2(filc_thread* my_thread, filc_ptr p0, unsigned long long p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMArrayType2((LLVMTypeRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMBFloatTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBFloatTypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBasicBlockAsValue(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBasicBlockAsValue((LLVMBasicBlockRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBlockAddress(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBlockAddress((LLVMValueRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAShr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildAShr((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildAdd((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAddrSpaceCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildAddrSpaceCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAggregateRet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildAggregateRet((LLVMBuilderRef)handle_of(a0), a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAlloca(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildAlloca((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAnd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildAnd((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildArrayAlloca(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildArrayAlloca((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildArrayMalloc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildArrayMalloc((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAtomicCmpXchg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, unsigned p5, bool p6)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildAtomicCmpXchg((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), p4, p5, p6);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAtomicCmpXchgSyncScope(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, unsigned p5, unsigned p6)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildAtomicCmpXchgSyncScope((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), p4, p5, p6);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAtomicRMW(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, unsigned p4, bool p5)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildAtomicRMW((LLVMBuilderRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), p4, p5);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAtomicRMWSyncScope(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, unsigned p4, unsigned p5)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildAtomicRMWSyncScope((LLVMBuilderRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), p4, p5);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildBinOp(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildBinOp((LLVMBuilderRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildBitCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildBr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildBr((LLVMBuilderRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCall2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a5 = filc_check_and_get_tmp_str(my_thread, p5);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildCall2((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3, p4, a5);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCast(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildCast((LLVMBuilderRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2), (LLVMTypeRef)handle_of(a3), a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCatchPad(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildCatchPad((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2, p3, a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCatchRet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildCatchRet((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMBasicBlockRef)handle_of(a2));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCatchSwitch(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildCatchSwitch((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMBasicBlockRef)handle_of(a2), p3, a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCleanupPad(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildCleanupPad((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2, p3, a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCleanupRet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildCleanupRet((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMBasicBlockRef)handle_of(a2));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCondBr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildCondBr((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMBasicBlockRef)handle_of(a2), (LLVMBasicBlockRef)handle_of(a3));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildExactSDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildExactSDiv((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildExactUDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildExactUDiv((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildExtractElement(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildExtractElement((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildExtractValue(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildExtractValue((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), p2, a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFAdd((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFCmp(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFCmp((LLVMBuilderRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFDiv((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFMul((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFNeg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFNeg((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFPCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPExt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFPExt((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPToSI(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFPToSI((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPToUI(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFPToUI((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPTrunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFPTrunc((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFRem(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFRem((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFSub((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFence(filc_thread* my_thread, filc_ptr p0, unsigned p1, bool p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFence((LLVMBuilderRef)handle_of(a0), p1, p2, a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFenceSyncScope(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFenceSyncScope((LLVMBuilderRef)handle_of(a0), p1, p2, a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFree(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFree((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFreeze(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildFreeze((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a5 = filc_check_and_get_tmp_str(my_thread, p5);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildGEP2((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3, p4, a5);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildGEPWithNoWrapFlags(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5, unsigned p6)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a5 = filc_check_and_get_tmp_str(my_thread, p5);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildGEPWithNoWrapFlags((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3, p4, a5, p6);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildGlobalString(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildGlobalString((LLVMBuilderRef)handle_of(a0), a1, a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildGlobalStringPtr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildGlobalStringPtr((LLVMBuilderRef)handle_of(a0), a1, a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildICmp(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildICmp((LLVMBuilderRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildInBoundsGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a5 = filc_check_and_get_tmp_str(my_thread, p5);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildInBoundsGEP2((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3, p4, a5);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIndirectBr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildIndirectBr((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildInsertElement(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildInsertElement((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildInsertValue(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildInsertValue((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), p3, a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIntCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildIntCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIntCast2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, bool p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildIntCast2((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), p3, a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIntToPtr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildIntToPtr((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildInvoke2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5, filc_ptr p6, filc_ptr p7)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a5 = unwrap_obj(my_thread, p5, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a6 = unwrap_obj(my_thread, p6, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    const char* a7 = filc_check_and_get_tmp_str(my_thread, p7);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildInvoke2((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3, p4, (LLVMBasicBlockRef)handle_of(a5), (LLVMBasicBlockRef)handle_of(a6), a7);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIsNotNull(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildIsNotNull((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIsNull(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildIsNull((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildLShr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildLShr((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildLandingPad(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildLandingPad((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), p3, a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildLoad2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildLoad2((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMalloc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildMalloc((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMemCpy(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a5 = unwrap_obj(my_thread, p5, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildMemCpy((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), p2, (LLVMValueRef)handle_of(a3), p4, (LLVMValueRef)handle_of(a5));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMemMove(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a5 = unwrap_obj(my_thread, p5, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildMemMove((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), p2, (LLVMValueRef)handle_of(a3), p4, (LLVMValueRef)handle_of(a5));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMemSet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildMemSet((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), p4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildMul((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNSWAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildNSWAdd((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNSWMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildNSWMul((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNSWNeg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildNSWNeg((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNSWSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildNSWSub((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNUWAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildNUWAdd((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNUWMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildNUWMul((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNUWSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildNUWSub((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNeg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildNeg((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNot(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildNot((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildOr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildOr((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildPhi(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildPhi((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildPointerCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildPointerCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildPtrDiff2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildPtrDiff2((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildPtrToInt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildPtrToInt((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildResume(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildResume((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildRet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildRet((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildRetVoid(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildRetVoid((LLVMBuilderRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildSDiv((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSExt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildSExt((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSExtOrBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildSExtOrBitCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSIToFP(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildSIToFP((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSRem(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildSRem((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSelect(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildSelect((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildShl(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildShl((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildShuffleVector(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildShuffleVector((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildStore(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildStore((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildStructGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str(my_thread, p4);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildStructGEP2((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), p3, a4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildSub((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSwitch(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildSwitch((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMBasicBlockRef)handle_of(a2), p3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildTrunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildTrunc((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildTruncOrBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildTruncOrBitCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildUDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildUDiv((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildUIToFP(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildUIToFP((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildURem(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildURem((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildUnreachable(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildUnreachable((LLVMBuilderRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildVAArg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildVAArg((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildXor(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildXor((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildZExt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildZExt((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildZExtOrBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str(my_thread, p3);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuildZExtOrBitCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMBuilderGetDefaultFPMathTag(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    LLVMMetadataRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMBuilderGetDefaultFPMathTag((LLVMBuilderRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMBuilderSetDefaultFPMathTag(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMBuilderSetDefaultFPMathTag((LLVMBuilderRef)handle_of(a0), (LLVMMetadataRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

bool filc_native_LLVMCanValueUseFastMathFlags(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMCanValueUseFastMathFlags((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMClearInsertionPosition(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMClearInsertionPosition((LLVMBuilderRef)handle_of(a0));
    pas_lock_unlock(&context->lock);
}

filc_ptr filc_native_LLVMCloneModule(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMModuleRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMCloneModule((LLVMModuleRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_MODULE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstAdd((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstAddrSpaceCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstAddrSpaceCast((LLVMValueRef)handle_of(a0), (LLVMTypeRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstAllOnes(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstAllOnes((LLVMTypeRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstArray(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstArray((LLVMTypeRef)handle_of(a0), a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstArray2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned long long p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstArray2((LLVMTypeRef)handle_of(a0), a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstBitCast((LLVMValueRef)handle_of(a0), (LLVMTypeRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstExtractElement(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstExtractElement((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstGEP2((LLVMTypeRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2, p3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstGEPWithNoWrapFlags(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, unsigned p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstGEPWithNoWrapFlags((LLVMTypeRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2, p3, p4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstInBoundsGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstInBoundsGEP2((LLVMTypeRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2, p3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstInlineAsm(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, bool p3, bool p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstInlineAsm((LLVMTypeRef)handle_of(a0), a1, a2, p3, p4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstInsertElement(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstInsertElement((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstInt(filc_thread* my_thread, filc_ptr p0, unsigned long long p1, bool p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstInt((LLVMTypeRef)handle_of(a0), p1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

long long filc_native_LLVMConstIntGetSExtValue(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    long long result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstIntGetSExtValue((LLVMValueRef)handle_of(a0));
    long long result = (long long)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned long long filc_native_LLVMConstIntGetZExtValue(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned long long result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstIntGetZExtValue((LLVMValueRef)handle_of(a0));
    unsigned long long result = (unsigned long long)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstIntOfString(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstIntOfString((LLVMTypeRef)handle_of(a0), a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstIntOfStringAndSize(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, unsigned p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMConstIntOfStringAndSize: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstIntOfStringAndSize((LLVMTypeRef)handle_of(a0), a1, p2, p3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstIntToPtr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstIntToPtr((LLVMValueRef)handle_of(a0), (LLVMTypeRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstMul((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNSWAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstNSWAdd((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNSWMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstNSWMul((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNSWNeg(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstNSWNeg((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNSWSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstNSWSub((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNUWAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstNUWAdd((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNUWMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstNUWMul((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNUWSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstNUWSub((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNamedStruct(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstNamedStruct((LLVMTypeRef)handle_of(a0), a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNeg(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstNeg((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNot(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstNot((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNull(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstNull((LLVMTypeRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstPointerCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstPointerCast((LLVMValueRef)handle_of(a0), (LLVMTypeRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstPointerNull(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstPointerNull((LLVMTypeRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstPtrToInt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstPtrToInt((LLVMValueRef)handle_of(a0), (LLVMTypeRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstReal(filc_thread* my_thread, filc_ptr p0, double p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstReal((LLVMTypeRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstRealOfString(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstRealOfString((LLVMTypeRef)handle_of(a0), a1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstRealOfStringAndSize(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMConstRealOfStringAndSize: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstRealOfStringAndSize((LLVMTypeRef)handle_of(a0), a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstShuffleVector(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstShuffleVector((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMConstStringInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMConstStringInContext: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstStringInContext((LLVMContextRef)a0->handle, a1, p2, p3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstStringInContext2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2, bool p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMConstStringInContext2: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstStringInContext2((LLVMContextRef)a0->handle, a1, p2, p3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMConstStructInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMValueRef* a1 = (LLVMValueRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstStructInContext((LLVMContextRef)a0->handle, a1, p2, p3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstSub((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstTrunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstTrunc((LLVMValueRef)handle_of(a0), (LLVMTypeRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstTruncOrBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstTruncOrBitCast((LLVMValueRef)handle_of(a0), (LLVMTypeRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstVector(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    LLVMValueRef* a0 = (LLVMValueRef*)decode_handle_array(my_thread, p0, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstVector(a0, p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstXor(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstXor((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMConstantPtrAuth(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMConstantPtrAuth((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMContextSetDiscardValueNames(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    pas_lock_lock(&context->lock);
    api->LLVMContextSetDiscardValueNames((LLVMContextRef)a0->handle, p1);
    pas_lock_unlock(&context->lock);
}

bool filc_native_LLVMContextShouldDiscardValueNames(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMContextShouldDiscardValueNames((LLVMContextRef)a0->handle);
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMCountBasicBlocks(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMCountBasicBlocks((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMCountIncoming(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMCountIncoming((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMCountParamTypes(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMCountParamTypes((LLVMTypeRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMCountParams(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMCountParams((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMCountStructElementTypes(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMCountStructElementTypes((LLVMTypeRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateBasicBlockInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMCreateBasicBlockInContext((LLVMContextRef)a0->handle, a1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMCreateBuilderInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMBuilderRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMCreateBuilderInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BUILDER, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateEnumAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned long long p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMAttributeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMCreateEnumAttribute((LLVMContextRef)a0->handle, p1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateStringAttribute(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3, unsigned p4)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMCreateStringAttribute: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    const char* a3;
    if (!filc_ptr_ptr(p3)) {
        FILC_CHECK(p4 == 0, NULL, "LLVMCreateStringAttribute: null string with nonzero length.");
        a3 = NULL;
    } else {
        filc_check_read(p3, (size_t)p4);
        a3 = (const char*)filc_ptr_ptr(p3);
    }
    LLVMAttributeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMCreateStringAttribute((LLVMContextRef)a0->handle, a1, p2, a3, p4);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateTypeAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMAttributeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMCreateTypeAttribute((LLVMContextRef)a0->handle, p1, (LLVMTypeRef)handle_of(a2));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMDoubleTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMDoubleTypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMDumpModule(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMDumpModule((LLVMModuleRef)handle_of(a0));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMDumpType(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMDumpType((LLVMTypeRef)handle_of(a0));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMDumpValue(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMDumpValue((LLVMValueRef)handle_of(a0));
    pas_lock_unlock(&context->lock);
}


filc_ptr filc_native_LLVMFP128TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMFP128TypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMFloatTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMFloatTypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMFunctionType(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef* a1 = (LLVMTypeRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMFunctionType((LLVMTypeRef)handle_of(a0), a1, p2, p3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGEPGetNoWrapFlags(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMGEPNoWrapFlags result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGEPGetNoWrapFlags((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMGEPSetNoWrapFlags(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMGEPSetNoWrapFlags((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

filc_ptr filc_native_LLVMGetAggregateElement(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetAggregateElement((LLVMValueRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetAlignment(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetAlignment((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetAllocatedType(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetAllocatedType((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetArgOperand(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetArgOperand((LLVMValueRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetArrayLength(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetArrayLength((LLVMTypeRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned long long filc_native_LLVMGetArrayLength2(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    uint64_t result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetArrayLength2((LLVMTypeRef)handle_of(a0));
    unsigned long long result = (unsigned long long)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetAtomicRMWBinOp(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMAtomicRMWBinOp result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetAtomicRMWBinOp((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetAtomicSyncScopeID(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetAtomicSyncScopeID((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetAttributeCountAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetAttributeCountAtIndex((LLVMValueRef)handle_of(a0), p1);
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBasicBlockParent(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetBasicBlockParent((LLVMBasicBlockRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBasicBlockTerminator(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetBasicBlockTerminator((LLVMBasicBlockRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBlockAddressBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetBlockAddressBasicBlock((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBlockAddressFunction(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetBlockAddressFunction((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBuilderContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    LLVMContextRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetBuilderContext((LLVMBuilderRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_CONTEXT, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCallBrDefaultDest(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetCallBrDefaultDest((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCallBrIndirectDest(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetCallBrIndirectDest((LLVMValueRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetCallBrNumIndirectDests(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetCallBrNumIndirectDests((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetCallSiteAttributeCount(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetCallSiteAttributeCount((LLVMValueRef)handle_of(a0), p1);
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCallSiteEnumAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMAttributeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetCallSiteEnumAttribute((LLVMValueRef)handle_of(a0), p1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCallSiteStringAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, unsigned p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2;
    if (!filc_ptr_ptr(p2)) {
        FILC_CHECK(p3 == 0, NULL, "LLVMGetCallSiteStringAttribute: null string with nonzero length.");
        a2 = NULL;
    } else {
        filc_check_read(p2, (size_t)p3);
        a2 = (const char*)filc_ptr_ptr(p2);
    }
    LLVMAttributeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetCallSiteStringAttribute((LLVMValueRef)handle_of(a0), p1, a2, p3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCalledFunctionType(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetCalledFunctionType((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCalledValue(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetCalledValue((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetCastOpcode(filc_thread* my_thread, filc_ptr p0, bool p1, filc_ptr p2, bool p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMOpcode result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetCastOpcode((LLVMValueRef)handle_of(a0), p1, (LLVMTypeRef)handle_of(a2), p3);
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetClause(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetClause((LLVMValueRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetCmpXchgFailureOrdering(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMAtomicOrdering result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetCmpXchgFailureOrdering((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetCmpXchgSuccessOrdering(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMAtomicOrdering result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetCmpXchgSuccessOrdering((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCondition(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetCondition((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetConstOpcode(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMOpcode result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetConstOpcode((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetConstantPtrAuthAddrDiscriminator(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetConstantPtrAuthAddrDiscriminator((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetConstantPtrAuthDiscriminator(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetConstantPtrAuthDiscriminator((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetConstantPtrAuthKey(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetConstantPtrAuthKey((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetConstantPtrAuthPointer(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetConstantPtrAuthPointer((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCurrentDebugLocation(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetCurrentDebugLocation((LLVMBuilderRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCurrentDebugLocation2(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    LLVMMetadataRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetCurrentDebugLocation2((LLVMBuilderRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetDLLStorageClass(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMDLLStorageClass result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetDLLStorageClass((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetDebugLocColumn(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetDebugLocColumn((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetDebugLocLine(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetDebugLocLine((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetElementType(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetElementType((LLVMTypeRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetEntryBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetEntryBasicBlock((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetEnumAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMAttributeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetEnumAttributeAtIndex((LLVMValueRef)handle_of(a0), p1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetEnumAttributeKind(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetEnumAttributeKind((LLVMAttributeRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetEnumAttributeKindForName(filc_thread* my_thread, filc_ptr p0, size_t p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    const char* a0;
    if (!filc_ptr_ptr(p0)) {
        FILC_CHECK(p1 == 0, NULL, "LLVMGetEnumAttributeKindForName: null string with nonzero length.");
        a0 = NULL;
    } else {
        filc_check_read(p0, (size_t)p1);
        a0 = (const char*)filc_ptr_ptr(p0);
    }
    return api->LLVMGetEnumAttributeKindForName(a0, p1);
}

unsigned long long filc_native_LLVMGetEnumAttributeValue(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    uint64_t result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetEnumAttributeValue((LLVMAttributeRef)handle_of(a0));
    unsigned long long result = (unsigned long long)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMGetExact(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetExact((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetFCmpPredicate(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMRealPredicate result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetFCmpPredicate((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetFastMathFlags(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMFastMathFlags result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetFastMathFlags((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetFirstBasicBlock((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstDbgRecord(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMDbgRecordRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetFirstDbgRecord((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_DBG_RECORD, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstFunction(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetFirstFunction((LLVMModuleRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstGlobal(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetFirstGlobal((LLVMModuleRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetFirstGlobalAlias((LLVMModuleRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetFirstGlobalIFunc((LLVMModuleRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstInstruction(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetFirstInstruction((LLVMBasicBlockRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstNamedMetadata(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMNamedMDNodeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetFirstNamedMetadata((LLVMModuleRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstParam(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetFirstParam((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstUse(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMUseRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetFirstUse((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_USE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetFunctionCallConv(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetFunctionCallConv((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetGEPSourceElementType(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetGEPSourceElementType((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetGlobalIFuncResolver(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetGlobalIFuncResolver((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetGlobalParent(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMModuleRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetGlobalParent((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_MODULE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetICmpPredicate(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMIntPredicate result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetICmpPredicate((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetIncomingBlock(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetIncomingBlock((LLVMValueRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetIncomingValue(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetIncomingValue((LLVMValueRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInitializer(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetInitializer((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInlineAsm(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2, filc_ptr p3, size_t p4, bool p5, bool p6, unsigned p7, bool p8)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetInlineAsm: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    const char* a3;
    if (!filc_ptr_ptr(p3)) {
        FILC_CHECK(p4 == 0, NULL, "LLVMGetInlineAsm: null string with nonzero length.");
        a3 = NULL;
    } else {
        filc_check_read(p3, (size_t)p4);
        a3 = (const char*)filc_ptr_ptr(p3);
    }
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetInlineAsm((LLVMTypeRef)handle_of(a0), a1, p2, a3, p4, p5, p6, p7, p8);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMGetInlineAsmCanUnwind(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetInlineAsmCanUnwind((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetInlineAsmDialect(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMInlineAsmDialect result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetInlineAsmDialect((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInlineAsmFunctionType(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetInlineAsmFunctionType((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMGetInlineAsmHasSideEffects(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetInlineAsmHasSideEffects((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMGetInlineAsmNeedsAlignedStack(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetInlineAsmNeedsAlignedStack((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInsertBlock(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetInsertBlock((LLVMBuilderRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetInstructionCallConv(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetInstructionCallConv((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetInstructionOpcode(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMOpcode result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetInstructionOpcode((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInstructionParent(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetInstructionParent((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetIntTypeWidth(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetIntTypeWidth((LLVMTypeRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetIntrinsicDeclaration(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, size_t p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMTypeRef* a2 = (LLVMTypeRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetIntrinsicDeclaration((LLVMModuleRef)handle_of(a0), p1, a2, p3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetIntrinsicID(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetIntrinsicID((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMGetIsDisjoint(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetIsDisjoint((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetLastBasicBlock((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastDbgRecord(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMDbgRecordRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetLastDbgRecord((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_DBG_RECORD, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetLastEnumAttributeKind(filc_thread* my_thread)
{
    ensure_api(my_thread);
    return api->LLVMGetLastEnumAttributeKind();
}

filc_ptr filc_native_LLVMGetLastFunction(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetLastFunction((LLVMModuleRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastGlobal(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetLastGlobal((LLVMModuleRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetLastGlobalAlias((LLVMModuleRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetLastGlobalIFunc((LLVMModuleRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastInstruction(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetLastInstruction((LLVMBasicBlockRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastNamedMetadata(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMNamedMDNodeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetLastNamedMetadata((LLVMModuleRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastParam(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetLastParam((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetLinkage(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMLinkage result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetLinkage((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}


unsigned filc_native_LLVMGetMDKindIDInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetMDKindIDInContext: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetMDKindIDInContext((LLVMContextRef)a0->handle, a1, p2);
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetMDNodeNumOperands(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetMDNodeNumOperands((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

int filc_native_LLVMGetMaskValue(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    int result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetMaskValue((LLVMValueRef)handle_of(a0), p1);
    int result = (int)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetMetadata((LLVMValueRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetModuleContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMContextRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetModuleContext((LLVMModuleRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_CONTEXT, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetModuleFlag(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetModuleFlag: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    LLVMMetadataRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetModuleFlag((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMGetNNeg(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNNeg((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMGetNSW(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNSW((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMGetNUW(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNUW((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedFunction(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNamedFunction((LLVMModuleRef)handle_of(a0), a1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedFunctionWithLength(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetNamedFunctionWithLength: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNamedFunctionWithLength((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedGlobal(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNamedGlobal((LLVMModuleRef)handle_of(a0), a1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedGlobalAlias(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetNamedGlobalAlias: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNamedGlobalAlias((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedGlobalIFunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetNamedGlobalIFunc: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNamedGlobalIFunc((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedGlobalWithLength(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetNamedGlobalWithLength: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNamedGlobalWithLength((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedMetadata(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetNamedMetadata: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    LLVMNamedMDNodeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNamedMetadata((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetNamedMetadataNumOperands(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNamedMetadataNumOperands((LLVMModuleRef)handle_of(a0), a1);
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNextBasicBlock((LLVMBasicBlockRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextDbgRecord(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_DBG_RECORD, &context, &module);
    LLVMDbgRecordRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNextDbgRecord((LLVMDbgRecordRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_DBG_RECORD, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextFunction(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNextFunction((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextGlobal(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNextGlobal((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNextGlobalAlias((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNextGlobalIFunc((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextInstruction(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNextInstruction((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextNamedMetadata(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_NAMED_METADATA, &context, &module);
    LLVMNamedMDNodeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNextNamedMetadata((LLVMNamedMDNodeRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextParam(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNextParam((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextUse(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_USE, &context, &module);
    LLVMUseRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNextUse((LLVMUseRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_USE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNormalDest(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNormalDest((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetNumArgOperands(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNumArgOperands((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetNumClauses(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNumClauses((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetNumContainedTypes(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNumContainedTypes((LLVMTypeRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetNumHandlers(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNumHandlers((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetNumIndices(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNumIndices((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetNumMaskElements(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNumMaskElements((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

int filc_native_LLVMGetNumOperands(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    int result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNumOperands((LLVMValueRef)handle_of(a0));
    int result = (int)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetNumSuccessors(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetNumSuccessors((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetOperand(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetOperand((LLVMValueRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetOperandUse(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMUseRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetOperandUse((LLVMValueRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_USE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetOrInsertNamedMetadata(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetOrInsertNamedMetadata: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    LLVMNamedMDNodeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetOrInsertNamedMetadata((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetOrdering(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMAtomicOrdering result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetOrdering((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetParam(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetParam((LLVMValueRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetParamParent(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetParamParent((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetParentCatchSwitch(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetParentCatchSwitch((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPersonalityFn(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetPersonalityFn((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetPointerAddressSpace(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetPointerAddressSpace((LLVMTypeRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPoison(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetPoison((LLVMTypeRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPrefixData(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetPrefixData((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetPreviousBasicBlock((LLVMBasicBlockRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousDbgRecord(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_DBG_RECORD, &context, &module);
    LLVMDbgRecordRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetPreviousDbgRecord((LLVMDbgRecordRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_DBG_RECORD, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousFunction(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetPreviousFunction((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousGlobal(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetPreviousGlobal((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetPreviousGlobalAlias((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetPreviousGlobalIFunc((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousInstruction(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetPreviousInstruction((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousNamedMetadata(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_NAMED_METADATA, &context, &module);
    LLVMNamedMDNodeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetPreviousNamedMetadata((LLVMNamedMDNodeRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousParam(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetPreviousParam((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPrologueData(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetPrologueData((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetReturnType(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetReturnType((LLVMTypeRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetStringAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, unsigned p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2;
    if (!filc_ptr_ptr(p2)) {
        FILC_CHECK(p3 == 0, NULL, "LLVMGetStringAttributeAtIndex: null string with nonzero length.");
        a2 = NULL;
    } else {
        filc_check_read(p2, (size_t)p3);
        a2 = (const char*)filc_ptr_ptr(p2);
    }
    LLVMAttributeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetStringAttributeAtIndex((LLVMValueRef)handle_of(a0), p1, a2, p3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetSuccessor(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetSuccessor((LLVMValueRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetSwitchDefaultDest(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetSwitchDefaultDest((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetSyncScopeID(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetSyncScopeID: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetSyncScopeID((LLVMContextRef)a0->handle, a1, p2);
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetTailCallKind(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMTailCallKind result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetTailCallKind((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetTargetExtTypeIntParam(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetTargetExtTypeIntParam((LLVMTypeRef)handle_of(a0), p1);
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetTargetExtTypeNumIntParams(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetTargetExtTypeNumIntParams((LLVMTypeRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetTargetExtTypeNumTypeParams(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetTargetExtTypeNumTypeParams((LLVMTypeRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTargetExtTypeTypeParam(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetTargetExtTypeTypeParam((LLVMTypeRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetThreadLocalMode(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMThreadLocalMode result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetThreadLocalMode((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTypeAttributeValue(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetTypeAttributeValue((LLVMAttributeRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTypeByName(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetTypeByName((LLVMModuleRef)handle_of(a0), a1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTypeByName2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetTypeByName2((LLVMContextRef)a0->handle, a1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTypeContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMContextRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetTypeContext((LLVMTypeRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_CONTEXT, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetTypeKind(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeKind result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetTypeKind((LLVMTypeRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetUndef(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetUndef((LLVMTypeRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

int filc_native_LLVMGetUndefMaskElem(filc_thread* my_thread)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    int result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetUndefMaskElem();
    int result = (int)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetUnnamedAddress(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMUnnamedAddr result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetUnnamedAddress((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetUnwindDest(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetUnwindDest((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetUsedValue(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_USE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetUsedValue((LLVMUseRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetUser(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_USE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetUser((LLVMUseRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGetValueContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMContextRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetValueContext((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_CONTEXT, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetValueKind(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueKind result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetValueKind((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetVectorSize(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetVectorSize((LLVMTypeRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMGetVisibility(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMVisibility result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetVisibility((LLVMValueRef)handle_of(a0));
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMGetVolatile(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetVolatile((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMGetWeak(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGetWeak((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMGlobalGetValueType(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMGlobalGetValueType((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMGlobalSetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMGlobalSetMetadata((LLVMValueRef)handle_of(a0), p1, (LLVMMetadataRef)handle_of(a2));
    pas_lock_unlock(&context->lock);
}


filc_ptr filc_native_LLVMHalfTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMHalfTypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

int filc_native_LLVMHasMetadata(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    int result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMHasMetadata((LLVMValueRef)handle_of(a0));
    int result = (int)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMHasPersonalityFn(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMHasPersonalityFn((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMHasPrefixData(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMHasPrefixData((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMHasPrologueData(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMHasPrologueData((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMHasUnnamedAddr(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMHasUnnamedAddr((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMInsertBasicBlockInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMInsertBasicBlockInContext((LLVMContextRef)a0->handle, (LLVMBasicBlockRef)handle_of(a1), a2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMInsertExistingBasicBlockAfterInsertBlock(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMInsertExistingBasicBlockAfterInsertBlock((LLVMBuilderRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMInsertIntoBuilder(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMInsertIntoBuilder((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    if (module)
        builder_adopt_module(my_thread, a0, module);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMInsertIntoBuilderWithName(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str(my_thread, p2);
    pas_lock_lock(&context->lock);
    api->LLVMInsertIntoBuilderWithName((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    if (module)
        builder_adopt_module(my_thread, a0, module);
    pas_lock_unlock(&context->lock);
}

filc_ptr filc_native_LLVMInstructionClone(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMInstructionClone((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMInt128TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMInt128TypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMInt16TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMInt16TypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMInt1TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMInt1TypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMInt32TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMInt32TypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMInt64TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMInt64TypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMInt8TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMInt8TypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMIntTypeInContext(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIntTypeInContext((LLVMContextRef)a0->handle, p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIntrinsicGetType(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, size_t p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef* a2 = (LLVMTypeRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIntrinsicGetType((LLVMContextRef)a0->handle, p1, a2, p3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIntrinsicIsOverloaded(filc_thread* my_thread, unsigned p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIntrinsicIsOverloaded(p0);
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAAddrSpaceCastInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAAddrSpaceCastInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAAllocaInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAAllocaInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAArgument(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAArgument((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAAtomicCmpXchgInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAAtomicCmpXchgInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAAtomicRMWInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAAtomicRMWInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsABasicBlock((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABinaryOperator(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsABinaryOperator((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABitCastInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsABitCastInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABlockAddress(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsABlockAddress((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABranchInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsABranchInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACallBrInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsACallBrInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACallInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsACallInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACastInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsACastInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACatchPadInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsACatchPadInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACatchReturnInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsACatchReturnInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACatchSwitchInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsACatchSwitchInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACleanupPadInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsACleanupPadInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACleanupReturnInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsACleanupReturnInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACmpInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsACmpInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstant(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAConstant((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantAggregateZero(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAConstantAggregateZero((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantArray(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAConstantArray((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantDataArray(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAConstantDataArray((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantDataSequential(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAConstantDataSequential((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantDataVector(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAConstantDataVector((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantExpr(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAConstantExpr((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantFP(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAConstantFP((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantInt(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAConstantInt((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantPointerNull(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAConstantPointerNull((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantPtrAuth(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAConstantPtrAuth((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantStruct(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAConstantStruct((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantTokenNone(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAConstantTokenNone((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantVector(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAConstantVector((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsADbgDeclareInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsADbgDeclareInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsADbgInfoIntrinsic(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsADbgInfoIntrinsic((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsADbgLabelInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsADbgLabelInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsADbgVariableIntrinsic(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsADbgVariableIntrinsic((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAExtractElementInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAExtractElementInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAExtractValueInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAExtractValueInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFCmpInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAFCmpInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFPExtInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAFPExtInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFPToSIInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAFPToSIInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFPToUIInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAFPToUIInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFPTruncInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAFPTruncInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFenceInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAFenceInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFreezeInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAFreezeInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFuncletPadInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAFuncletPadInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFunction(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAFunction((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGetElementPtrInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAGetElementPtrInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAGlobalAlias((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAGlobalIFunc((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalObject(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAGlobalObject((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalValue(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAGlobalValue((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalVariable(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAGlobalVariable((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAICmpInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAICmpInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAIndirectBrInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAIndirectBrInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInlineAsm(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAInlineAsm((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInsertElementInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAInsertElementInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInsertValueInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAInsertValueInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInstruction(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAInstruction((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAIntToPtrInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAIntToPtrInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAIntrinsicInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAIntrinsicInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInvokeInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAInvokeInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsALandingPadInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsALandingPadInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsALoadInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsALoadInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAMemCpyInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAMemCpyInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAMemIntrinsic(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAMemIntrinsic((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAMemMoveInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAMemMoveInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAMemSetInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAMemSetInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAPHINode(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAPHINode((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAPoisonValue(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAPoisonValue((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAPtrToIntInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAPtrToIntInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAResumeInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAResumeInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAReturnInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAReturnInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsASExtInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsASExtInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsASIToFPInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsASIToFPInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsASelectInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsASelectInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAShuffleVectorInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAShuffleVectorInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAStoreInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAStoreInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsASwitchInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsASwitchInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsATruncInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsATruncInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUIToFPInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAUIToFPInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUnaryInstruction(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAUnaryInstruction((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUnaryOperator(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAUnaryOperator((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUndefValue(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAUndefValue((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUnreachableInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAUnreachableInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUser(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAUser((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAVAArgInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAVAArgInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAZExtInst(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsAZExtInst((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsCleanup(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsCleanup((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsConditional(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsConditional((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsConstant(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsConstant((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsConstantString(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsConstantString((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsDeclaration(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsDeclaration((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsEnumAttribute(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsEnumAttribute((LLVMAttributeRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsExternallyInitialized(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsExternallyInitialized((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsFunctionVarArg(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsFunctionVarArg((LLVMTypeRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsGlobalConstant(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsGlobalConstant((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsInBounds(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsInBounds((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsLiteralStruct(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsLiteralStruct((LLVMTypeRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsMultithreaded(filc_thread* my_thread)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsMultithreaded();
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsNewDbgInfoFormat(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsNewDbgInfoFormat((LLVMModuleRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsNull(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsNull((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsOpaqueStruct(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsOpaqueStruct((LLVMTypeRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsPackedStruct(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsPackedStruct((LLVMTypeRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsPoison(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsPoison((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsStringAttribute(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsStringAttribute((LLVMAttributeRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsTailCall(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsTailCall((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsThreadLocal(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsThreadLocal((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsTypeAttribute(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsTypeAttribute((LLVMAttributeRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMIsUndef(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMIsUndef((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMLabelTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMLabelTypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMLookupIntrinsicID(filc_thread* my_thread, filc_ptr p0, size_t p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    const char* a0;
    if (!filc_ptr_ptr(p0)) {
        FILC_CHECK(p1 == 0, NULL, "LLVMLookupIntrinsicID: null string with nonzero length.");
        a0 = NULL;
    } else {
        filc_check_read(p0, (size_t)p1);
        a0 = (const char*)filc_ptr_ptr(p0);
    }
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMLookupIntrinsicID(a0, p1);
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMMDNodeInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMValueRef* a1 = (LLVMValueRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMMDNodeInContext((LLVMContextRef)a0->handle, a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMMDNodeInContext2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMMetadataRef* a1 = (LLVMMetadataRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    LLVMMetadataRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMMDNodeInContext2((LLVMContextRef)a0->handle, a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMMDStringInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMMDStringInContext: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMMDStringInContext((LLVMContextRef)a0->handle, a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMMDStringInContext2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMMDStringInContext2: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    LLVMMetadataRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMMDStringInContext2((LLVMContextRef)a0->handle, a1, p2);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMMetadataAsValue(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMMetadataAsValue((LLVMContextRef)a0->handle, (LLVMMetadataRef)handle_of(a1));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMMetadataTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMMetadataTypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMModuleCreateWithNameInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    const char* a0 = filc_check_and_get_tmp_str(my_thread, p0);
    filc_llvm_jit_context_object* a1 = unwrap_context(my_thread, p1);
    context = a1;
    LLVMModuleRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMModuleCreateWithNameInContext(a0, (LLVMContextRef)a1->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_MODULE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMModuleFlagEntriesGetFlagBehavior(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES, &context, &module);
    LLVMModuleFlagBehavior result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMModuleFlagEntriesGetFlagBehavior((LLVMModuleFlagEntry*)handle_of(a0), p1);
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMModuleFlagEntriesGetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES, &context, &module);
    LLVMMetadataRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMModuleFlagEntriesGetMetadata((LLVMModuleFlagEntry*)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMMoveBasicBlockAfter(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMMoveBasicBlockAfter((LLVMBasicBlockRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMMoveBasicBlockBefore(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMMoveBasicBlockBefore((LLVMBasicBlockRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}


filc_ptr filc_native_LLVMPPCFP128TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMPPCFP128TypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMPointerType(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMPointerType((LLVMTypeRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMPointerTypeInContext(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMPointerTypeInContext((LLVMContextRef)a0->handle, p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMPointerTypeIsOpaque(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMPointerTypeIsOpaque((LLVMTypeRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMPositionBuilder(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMPositionBuilder((LLVMBuilderRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1), (LLVMValueRef)handle_of(a2));
    if (module)
        builder_adopt_module(my_thread, a0, module);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMPositionBuilderAtEnd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMPositionBuilderAtEnd((LLVMBuilderRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    if (module)
        builder_adopt_module(my_thread, a0, module);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMPositionBuilderBefore(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMPositionBuilderBefore((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    if (module)
        builder_adopt_module(my_thread, a0, module);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMPositionBuilderBeforeDbgRecords(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMPositionBuilderBeforeDbgRecords((LLVMBuilderRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1), (LLVMValueRef)handle_of(a2));
    if (module)
        builder_adopt_module(my_thread, a0, module);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMPositionBuilderBeforeInstrAndDbgRecords(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMPositionBuilderBeforeInstrAndDbgRecords((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    if (module)
        builder_adopt_module(my_thread, a0, module);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMRemoveCallSiteEnumAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMRemoveCallSiteEnumAttribute((LLVMValueRef)handle_of(a0), p1, p2);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMRemoveCallSiteStringAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, unsigned p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2;
    if (!filc_ptr_ptr(p2)) {
        FILC_CHECK(p3 == 0, NULL, "LLVMRemoveCallSiteStringAttribute: null string with nonzero length.");
        a2 = NULL;
    } else {
        filc_check_read(p2, (size_t)p3);
        a2 = (const char*)filc_ptr_ptr(p2);
    }
    pas_lock_lock(&context->lock);
    api->LLVMRemoveCallSiteStringAttribute((LLVMValueRef)handle_of(a0), p1, a2, p3);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMRemoveEnumAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMRemoveEnumAttributeAtIndex((LLVMValueRef)handle_of(a0), p1, p2);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMRemoveStringAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, unsigned p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2;
    if (!filc_ptr_ptr(p2)) {
        FILC_CHECK(p3 == 0, NULL, "LLVMRemoveStringAttributeAtIndex: null string with nonzero length.");
        a2 = NULL;
    } else {
        filc_check_read(p2, (size_t)p3);
        a2 = (const char*)filc_ptr_ptr(p2);
    }
    pas_lock_lock(&context->lock);
    api->LLVMRemoveStringAttributeAtIndex((LLVMValueRef)handle_of(a0), p1, a2, p3);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMReplaceAllUsesWith(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMReplaceAllUsesWith((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMReplaceMDNodeOperandWith(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMReplaceMDNodeOperandWith((LLVMValueRef)handle_of(a0), p1, (LLVMMetadataRef)handle_of(a2));
    pas_lock_unlock(&context->lock);
}

filc_ptr filc_native_LLVMScalableVectorType(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMScalableVectorType((LLVMTypeRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMSetAlignment(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetAlignment((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetArgOperand(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetArgOperand((LLVMValueRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetAtomicRMWBinOp(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetAtomicRMWBinOp((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetAtomicSingleThread(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetAtomicSingleThread((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetAtomicSyncScopeID(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetAtomicSyncScopeID((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetCleanup(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetCleanup((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetCmpXchgFailureOrdering(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetCmpXchgFailureOrdering((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetCmpXchgSuccessOrdering(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetCmpXchgSuccessOrdering((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetCondition(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetCondition((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetCurrentDebugLocation(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetCurrentDebugLocation((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetCurrentDebugLocation2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetCurrentDebugLocation2((LLVMBuilderRef)handle_of(a0), (LLVMMetadataRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetDLLStorageClass(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetDLLStorageClass((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetDataLayout(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    pas_lock_lock(&context->lock);
    api->LLVMSetDataLayout((LLVMModuleRef)handle_of(a0), a1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetExact(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetExact((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetExternallyInitialized(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetExternallyInitialized((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetFastMathFlags(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetFastMathFlags((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetFunctionCallConv(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetFunctionCallConv((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetGC(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    pas_lock_lock(&context->lock);
    api->LLVMSetGC((LLVMValueRef)handle_of(a0), a1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetGlobalConstant(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetGlobalConstant((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetGlobalIFuncResolver(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetGlobalIFuncResolver((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetInitializer(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetInitializer((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetInstDebugLocation(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetInstDebugLocation((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetInstrParamAlignment(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetInstrParamAlignment((LLVMValueRef)handle_of(a0), p1, p2);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetInstructionCallConv(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetInstructionCallConv((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetIsDisjoint(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetIsDisjoint((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetIsInBounds(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetIsInBounds((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetIsNewDbgInfoFormat(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetIsNewDbgInfoFormat((LLVMModuleRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetLinkage(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetLinkage((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetMetadata((LLVMValueRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetModuleIdentifier(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMSetModuleIdentifier: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    pas_lock_lock(&context->lock);
    api->LLVMSetModuleIdentifier((LLVMModuleRef)handle_of(a0), a1, p2);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetModuleInlineAsm(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    pas_lock_lock(&context->lock);
    api->LLVMSetModuleInlineAsm((LLVMModuleRef)handle_of(a0), a1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetModuleInlineAsm2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMSetModuleInlineAsm2: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    pas_lock_lock(&context->lock);
    api->LLVMSetModuleInlineAsm2((LLVMModuleRef)handle_of(a0), a1, p2);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetNNeg(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetNNeg((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetNSW(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetNSW((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetNUW(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetNUW((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetNormalDest(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetNormalDest((LLVMValueRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetOperand(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetOperand((LLVMValueRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetOrdering(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetOrdering((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetParamAlignment(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetParamAlignment((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetParentCatchSwitch(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetParentCatchSwitch((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetPersonalityFn(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetPersonalityFn((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetPrefixData(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetPrefixData((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetPrologueData(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetPrologueData((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetSection(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    pas_lock_lock(&context->lock);
    api->LLVMSetSection((LLVMValueRef)handle_of(a0), a1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetSourceFileName(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMSetSourceFileName: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    pas_lock_lock(&context->lock);
    api->LLVMSetSourceFileName((LLVMModuleRef)handle_of(a0), a1, p2);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetSuccessor(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetSuccessor((LLVMValueRef)handle_of(a0), p1, (LLVMBasicBlockRef)handle_of(a2));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetTailCall(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetTailCall((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetTailCallKind(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetTailCallKind((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetTarget(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    pas_lock_lock(&context->lock);
    api->LLVMSetTarget((LLVMModuleRef)handle_of(a0), a1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetThreadLocal(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetThreadLocal((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetThreadLocalMode(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetThreadLocalMode((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetUnnamedAddr(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetUnnamedAddr((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetUnnamedAddress(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetUnnamedAddress((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetUnwindDest(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetUnwindDest((LLVMValueRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetValueName(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    pas_lock_lock(&context->lock);
    api->LLVMSetValueName((LLVMValueRef)handle_of(a0), a1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetValueName2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a1;
    if (!filc_ptr_ptr(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMSetValueName2: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    pas_lock_lock(&context->lock);
    api->LLVMSetValueName2((LLVMValueRef)handle_of(a0), a1, p2);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetVisibility(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetVisibility((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetVolatile(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetVolatile((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMSetWeak(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMSetWeak((LLVMValueRef)handle_of(a0), p1);
    pas_lock_unlock(&context->lock);
}

filc_ptr filc_native_LLVMSizeOf(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMSizeOf((LLVMTypeRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMStructCreateNamed(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    const char* a1 = filc_check_and_get_tmp_str(my_thread, p1);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMStructCreateNamed((LLVMContextRef)a0->handle, a1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMStructGetTypeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMStructGetTypeAtIndex((LLVMTypeRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_native_LLVMStructSetBody(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef* a1 = (LLVMTypeRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    pas_lock_lock(&context->lock);
    api->LLVMStructSetBody((LLVMTypeRef)handle_of(a0), a1, p2, p3);
    pas_lock_unlock(&context->lock);
}


filc_ptr filc_native_LLVMStructTypeInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef* a1 = (LLVMTypeRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMStructTypeInContext((LLVMContextRef)a0->handle, a1, p2, p3);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMTokenTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMTokenTypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMTypeIsSized(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMTypeIsSized((LLVMTypeRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMTypeOf(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMTypeOf((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMValueAsBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMValueAsBasicBlock((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMValueAsMetadata(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMMetadataRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMValueAsMetadata((LLVMValueRef)handle_of(a0));
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

bool filc_native_LLVMValueIsBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBool result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMValueIsBasicBlock((LLVMValueRef)handle_of(a0));
    bool result = (bool)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

unsigned filc_native_LLVMValueMetadataEntriesGetKind(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_METADATA_ENTRIES, &context, &module);
    unsigned result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMValueMetadataEntriesGetKind((LLVMValueMetadataEntry*)handle_of(a0), p1);
    unsigned result = (unsigned)result_raw;
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMValueMetadataEntriesGetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_METADATA_ENTRIES, &context, &module);
    LLVMMetadataRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMValueMetadataEntriesGetMetadata((LLVMValueMetadataEntry*)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

filc_ptr filc_native_LLVMVectorType(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMVectorType((LLVMTypeRef)handle_of(a0), p1);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMVoidTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMVoidTypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMX86AMXTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMX86AMXTypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}


filc_ptr filc_native_LLVMX86FP80TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->lock);
    result_raw = api->LLVMX86FP80TypeInContext((LLVMContextRef)a0->handle);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->lock);
    return result;
}

void filc_stack_overflow_failure(void); /* Not a real function, but we link it like one. */

static const shim_runtime_api runtime_api = {
#include "../../../filc/llvmjit/runtime_api_init.inc"
};

static bool try_load_one(const char* path)
{
    if (!path || !path[0])
        return false;
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle)
        return false;
    const struct shim_api* (*get_api)(const shim_runtime_api*, const char*) =
        (const struct shim_api* (*)(const shim_runtime_api*, const char*))
        dlsym(handle, "FilLLVMJIT_GetApi");
    if (!get_api) {
        dlclose(handle);
        return false;
    }
    const char* options;
    if (PAS_ENABLE_TESTING)
        options = secure_getenv("FILC_LLVM_OPTIONS");
    else
        options = NULL;
    const struct shim_api* my_api = get_api(&runtime_api, options);
    if (!my_api || my_api->version != SHIM_API_VERSION) {
        dlclose(handle);
        return false;
    }

    PAS_ASSERT(jit_heap_fresh_memory_should_be_committed());

    size_t size = 128 * 1024 * 1024;
    void* jit_memory = mmap(
        NULL, size, PROT_WRITE | PROT_READ | PROT_EXEC,
        MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
    PAS_ASSERT(jit_memory);
    PAS_ASSERT(jit_memory != (void*)(intptr_t)-1);
    jit_heap_add_fresh_memory(pas_range_create((uintptr_t)jit_memory, (uintptr_t)jit_memory + size));
    
    pas_store_store_fence();
    api = my_api;
    return true;
}

static void ensure_api_slow(filc_thread* my_thread)
{
    pas_lock_lock(&api_mutex);
    if (api) {
        pas_lock_unlock(&api_mutex);
        PAS_ASSERT(api);
        return;
    }
    const char* env = secure_getenv("FILC_LLVMJIT_SO");
    if (env && env[0])
        try_load_one(env);
    if (!api) {
        Dl_info info;
        if (dladdr((void*)ensure_api_slow, &info) && info.dli_fname) {
            char dir[4096];
            snprintf(dir, sizeof(dir), "%s", info.dli_fname);
            char* slash = strrchr(dir, '/');
            if (slash) {
                *slash = '\0';
                char candidate[4096];
                snprintf(candidate, sizeof(candidate),
                         "%s/../libexec/libfilcllvmjit.so", dir);
                if (!try_load_one(candidate)) {
                    const char* error = dlerror();
                    pas_lock_unlock(&api_mutex);
                    filc_safety_panic(NULL, "could not dlopen libfilcllvmjit.so: %s.",
                                      error ? error : "<unknown>");
                }
            }
        }
    }
    PAS_ASSERT(api);
    pas_lock_unlock(&api_mutex);
    PAS_UNUSED_PARAM(my_thread);
}

#else /* PAS_GLIBC -> so !PAS_GLIBC */

void filc_llvm_jit_destruct(void* payload)
{
    PAS_UNUSED_PARAM(payload);
    PAS_ASSERT(!"Should not be reached");
}

#endif /* PAS_GLIBC -> so end of !PAS_GLIBC */

#endif /* PAS_ENABLE_FILC */

#endif /* LIBPAS_ENABLED */
