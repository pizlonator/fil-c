/*
 * Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
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

bool filc_allow_jit = false;

/* We only support the LLVM JIT in glibc, because otherwise, there's no way to load the LLVM so,
   which is built against glibc.

   What a hack LMAO! */
#if PAS_GLIBC

#include "../../../filc/llvmjit/filc_llvm_jit_api.h"

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
    if (kind == FILC_LLVM_JIT_KIND_CONTEXT) {
        /* Fil-C: LLVMGetModuleContext / LLVMGetTypeContext /
         * LLVMGetValueContext / LLVMGetBuilderContext return the context
         * itself.  Handing back a FRESH wrapper object would break
         * identity with the context wrapper the caller already holds
         * (and the kind switch below never had a CONTEXT case -- latent
         * since these were declared, first hit by llvmpipe).  Return the
         * context's own object. */
        FILC_CHECK(context_obj, NULL,
                   "Fil-C LLVM JIT API: null LLVM context.");
        return filc_ptr_create_with_special_object(
            my_thread, filc_object_for_special_payload(context_obj));
    }
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
    filc_llvm_jit_builder_object* builder_obj = (filc_llvm_jit_builder_object*)builder;

    if (builder_obj->base.module) {
        FILC_CHECK(&builder_obj->base.module->base == module_obj, NULL,
                   "Fil-C LLVM JIT API: cannot move builder between modules");
        return;
    }

    filc_store_barrier(my_thread, filc_object_for_special_payload(module_obj));
    builder_obj->base.module = (filc_llvm_jit_module_object*)module_obj;
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
    /* Fil-C: compare contexts/modules by their RAW LLVM handles, not by
     * wrapper-object identity.  The runtime wraps every returned handle in
     * a fresh GC object (see wrap_handle), so two calls that return the
     * same LLVM module (e.g. LLVMGetGlobalParent over and over in mesa's
     * lp_build_intrinsic) yield different wrappers for one module; the
     * wrapper-pointer comparison turned that into a spurious "mixing
     * objects from different LLVM modules" panic (first hit by llvmpipe's
     * first shader build).  Raw handles preserve the check's purpose:
     * catching genuinely different modules/contexts in one call. */
    if (!*context_inout)
        *context_inout = payload->context;
    else {
        FILC_CHECK((*context_inout)->handle == payload->context->handle, NULL,
                   "Fil-C LLVM JIT API: mixing objects from different LLVM contexts.");
    }
    if (payload->module && module_inout) {
        if (!*module_inout)
            *module_inout = &payload->module->base;
        else {
            FILC_CHECK(((filc_llvm_jit_object*)*module_inout)->handle
                           == payload->module->base.handle, NULL,
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
        /* Fil-C: every MODULE-kind wrapper owns its LLVM module -- the only
         * wrappers that ever exist are the ones created by
         * LLVMModuleCreateWithNameInContext / LLVMCloneModule, plus whatever
         * LLVMGetGlobalParent hands back, which is the very same wrapper
         * object (see filc_native_LLVMGetGlobalParent), never a fresh one.
         * So exactly one wrapper per module dies exactly once. */
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
    PAS_ASSERT(session->kind == FILC_LLVM_JIT_KIND_JIT_SESSION);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module_obj =
        unwrap_obj(my_thread, module_ptr, FILC_LLVM_JIT_KIND_MODULE,
                   &context, NULL);
    PAS_ASSERT(context);
    /* FIXME: Would be cool if we exited around the compile. */
    pas_lock_lock(&session->lock);
    pas_lock_lock(&context->lock);

    filc_llvm_jit_executable_module* module = filc_object_special_payload(
        my_thread, filc_allocate_special_with_heap(
            my_thread, sizeof(filc_llvm_jit_executable_module), 1,
            FILC_SPECIAL_TYPE_LLVM_JIT, fugc_destructor_heap));
    module->kind = FILC_LLVM_JIT_KIND_EXECUTABLE_MODULE;
    pas_lock_construct(&module->lock);
    pas_lock_lock(&module->lock);
    filc_raw_ptr_array_construct(&module->jit_bases);
    filc_object_array_construct(&module->objects);
    filc_raw_ptr_array_construct(&module->sections);
    filc_raw_ptr_array_construct(&module->auxes);
    module->mapping = NULL;

    size_t index;
    for (index = filc_object_array_num_objects(&session->objects); index--;) {
        filc_store_barrier(my_thread, filc_object_array_at(&session->objects, index));
        filc_object_array_push(&module->objects, filc_object_array_at(&session->objects, index));
    }

    module->mapping = api->ShimJITCompile(my_thread, module, session->handle, module_obj->handle);

    pas_lock_unlock(&context->lock);
    pas_lock_unlock(&session->lock);

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
        uintptr_t start = (uintptr_t)module->jit_bases.array[index];
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

    pas_lock_unlock(&module->lock);

    filc_exit(my_thread);
    filc_soft_handshake(code_fence, NULL);
    filc_enter(my_thread);
    return filc_ptr_for_special_payload_with_manual_tracking(module);
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
    filc_alignment_header_construct_with_jit_module(my_thread, result, alignment, module);
    void* lower = (char*)result + offset_to_payload;
    filc_object* object = filc_object_for_lower(lower);
    /* We can't use the normal barrier because the normal barrier expects the object to have its
       header set up correctly; in particular, it expects that if the mark base is not the bottom of
       the object then the object header has the alignment set up.

       It would be silly to set up the alignment here just for the mark barrier. The actual header is
       built up by the LLVM linker and then the end of CompileModule.

       If we did have the header set up, then what the barrier would do is mark and enqueue it. We
       skip enqueueing it because we know that objects allocated by the JIT have an aux that is traced
       by the module, and though the object points at the module, we have already marked the module. */
    if (filc_current_marking_state)
        verse_heap_set_is_marked_relaxed(result, true);
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

static void shim_panic(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    filc_panic_impl(
        NULL, "filc llvm jit error", "thwarted a futile attempt to misuse the LLVM JIT API.",
        format, args);
}

/* Descriptor validation: a REF or ARR descriptor may carry a trailing check
 * token naming the C++ subclass that the LLVM C API implementation demands
 * (Core.cpp downcasts with unwrap<RequiredType>, which asserts -- or silently
 * type-confuses in a NDEBUG build -- when the wrapper hands it anything
 * else).  A descriptor without a token, and CTX/CSTR/OPTSTR/SCAL/ADOPT
 * descriptors generally, need no check: their filc kind already pins the
 * exact C++ type, they are scalars or strings, or the LLVM implementation is
 * graceful.  The VALIDATE stage runs after the unwraps and the lock and
 * before the api-> call, so that the subclass reads happen while the LLVM
 * context lock is held and a wrong subclass becomes a Fil-C panic instead of
 * an LLVM assertion abort.
 *
 * A check token is spelled FILC_LLVM_JIT_VAL_<token> for REF descriptors and
 * FILC_LLVM_JIT_VAL_ELEM_<token> for ARR descriptors (the element form exists
 * so that the message can say "element"); each expands to a whole statement
 * taking (wrapper name, raw handle), and the ARR element form additionally
 * takes the element count.  NONE means no check.  An ARR descriptor's token
 * also selects the null-element policy, FILC_LLVM_JIT_VAL_NULL_ELEM_<token>;
 * only ElemNullOK and ElemMDNodeOperand permit null elements.  The
 * hand-written wrappers below use the same check tokens directly, passing
 * the wrapper name as a bare identifier so that it is stringified into the
 * panic message. */

#define FILC_LLVM_JIT_VALIDATE(fname, d) \
    FILC_LLVM_JIT_VALIDATE_2(fname, FILC_LLVM_JIT_STRIP d)
#define FILC_LLVM_JIT_VALIDATE_2(fname, kind, ...) \
    FILC_LLVM_JIT_VALIDATE_3(fname, kind, ##__VA_ARGS__)
#define FILC_LLVM_JIT_VALIDATE_3(fname, kind, ...) \
    FILC_LLVM_JIT_VALIDATE_##kind(fname, ##__VA_ARGS__)
#define FILC_LLVM_JIT_VALIDATE_CTX(fname, name)
#define FILC_LLVM_JIT_VALIDATE_CSTR(fname, name)
#define FILC_LLVM_JIT_VALIDATE_OPTSTR(fname, name, len_name)
#define FILC_LLVM_JIT_VALIDATE_SCAL(fname, name, type)
#define FILC_LLVM_JIT_VALIDATE_ADOPT(fname, builder)

/* The trailing NONE is the default token: it is the marker itself when the
 * descriptor carries no check token, and a harmless extra argument absorbed
 * by the ... of the two-argument form when it does.  The dispatchers here do
 * not terminate the statement; the check tokens do. */
#define FILC_LLVM_JIT_VALIDATE_REF(fname, name, kind, ref_ty, ...) \
    FILC_LLVM_JIT_VALIDATE_REF_2(fname, name, ##__VA_ARGS__, NONE)
#define FILC_LLVM_JIT_VALIDATE_REF_2(fname, name, marker, ...) \
    FILC_LLVM_JIT_VAL_##marker(fname, name##_obj->handle)

/* An ARR descriptor validates per element even without a check token: most
 * of the LLVM implementations dereference the elements unconditionally,
 * while decode_handle_array tolerates null elements, so a null element is
 * checked for its own sake.  The exceptions are the two MDNode
 * constructors, which permit null elements: LLVMMDNodeInContext maps a
 * null element to null metadata, and LLVMMDNodeInContext2 forwards nulls to
 * MDNode::get.  The token selects both the null-element policy
 * (FILC_LLVM_JIT_VAL_NULL_ELEM_*, which skips the null check for ElemNullOK
 * and ElemMDNodeOperand) and the subclass check (FILC_LLVM_JIT_VAL_ELEM_*,
 * which ElemNullOK leaves empty and ElemMDNodeOperand fills with the
 * LLVMMDNodeInContext element rules). */
#define FILC_LLVM_JIT_VALIDATE_ARR(fname, name, count_name, kind, elem_ty, ...) \
    FILC_LLVM_JIT_VALIDATE_ARR_2(fname, name, count_name, ##__VA_ARGS__, NONE)
#define FILC_LLVM_JIT_VALIDATE_ARR_2(fname, name, count_name, marker, ...) \
    do { \
        size_t filc_llvm_jit_validate_index; \
        FILC_LLVM_JIT_VAL_NULL_ELEM_##marker(fname, name, count_name) \
        for (filc_llvm_jit_validate_index = 0; \
             filc_llvm_jit_validate_index < (size_t)(count_name); \
             ++filc_llvm_jit_validate_index) { \
            FILC_LLVM_JIT_VAL_ELEM_##marker( \
                fname, name##_obj[filc_llvm_jit_validate_index], count_name) \
        } \
    } while (0);

/* Check tokens.  Single-class checks are spelled with the article so that the
 * panic message reads like a sentence.  The single-IsA-class helper and the
 * delegating tokens below it do not terminate the statement; the FILC_CHECK
 * leaves do. */

#define FILC_LLVM_JIT_VAL_NONE(fname, handle)

#define FILC_LLVM_JIT_VAL_IS_A(fname, handle, article, class) \
    FILC_CHECK(api->LLVMIsA##class((LLVMValueRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected " #article " llvm::" #class ".");

#define FILC_LLVM_JIT_VAL_AllocaInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, an, AllocaInst)
#define FILC_LLVM_JIT_VAL_Argument(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, an, Argument)
#define FILC_LLVM_JIT_VAL_AtomicCmpXchgInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, an, AtomicCmpXchgInst)
#define FILC_LLVM_JIT_VAL_AtomicRMWInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, an, AtomicRMWInst)
#define FILC_LLVM_JIT_VAL_BasicBlock(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, BasicBlock)
#define FILC_LLVM_JIT_VAL_BlockAddress(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, BlockAddress)
#define FILC_LLVM_JIT_VAL_BranchInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, BranchInst)
#define FILC_LLVM_JIT_VAL_CallBrInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, CallBrInst)
#define FILC_LLVM_JIT_VAL_CallInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, CallInst)
#define FILC_LLVM_JIT_VAL_CatchPadInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, CatchPadInst)
#define FILC_LLVM_JIT_VAL_CatchSwitchInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, CatchSwitchInst)
#define FILC_LLVM_JIT_VAL_CleanupPadInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, CleanupPadInst)
#define FILC_LLVM_JIT_VAL_CleanupReturnInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, CleanupReturnInst)
#define FILC_LLVM_JIT_VAL_Constant(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, Constant)
#define FILC_LLVM_JIT_VAL_ConstantDataSequential(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, ConstantDataSequential)
#define FILC_LLVM_JIT_VAL_ConstantExpr(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, ConstantExpr)
#define FILC_LLVM_JIT_VAL_ConstantFP(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, ConstantFP)
#define FILC_LLVM_JIT_VAL_ConstantInt(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, ConstantInt)
#define FILC_LLVM_JIT_VAL_ConstantPtrAuth(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, ConstantPtrAuth)
#define FILC_LLVM_JIT_VAL_ExtractValueInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, an, ExtractValueInst)
#define FILC_LLVM_JIT_VAL_FenceInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, FenceInst)
#define FILC_LLVM_JIT_VAL_FuncletPadInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, FuncletPadInst)
#define FILC_LLVM_JIT_VAL_Function(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, Function)
#define FILC_LLVM_JIT_VAL_GetElementPtrInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, GetElementPtrInst)
#define FILC_LLVM_JIT_VAL_GlobalAlias(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, GlobalAlias)
#define FILC_LLVM_JIT_VAL_GlobalIFunc(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, GlobalIFunc)
#define FILC_LLVM_JIT_VAL_GlobalObject(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, GlobalObject)
#define FILC_LLVM_JIT_VAL_GlobalValue(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, GlobalValue)
#define FILC_LLVM_JIT_VAL_GlobalVariable(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, GlobalVariable)
#define FILC_LLVM_JIT_VAL_InlineAsm(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, an, InlineAsm)
#define FILC_LLVM_JIT_VAL_IndirectBrInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, an, IndirectBrInst)
#define FILC_LLVM_JIT_VAL_InsertValueInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, an, InsertValueInst)
#define FILC_LLVM_JIT_VAL_Instruction(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, an, Instruction)
#define FILC_LLVM_JIT_VAL_InvokeInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, an, InvokeInst)
#define FILC_LLVM_JIT_VAL_LandingPadInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, LandingPadInst)
#define FILC_LLVM_JIT_VAL_LoadInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, LoadInst)
#define FILC_LLVM_JIT_VAL_PHINode(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, PHINode)
#define FILC_LLVM_JIT_VAL_ShuffleVectorInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, ShuffleVectorInst)
#define FILC_LLVM_JIT_VAL_StoreInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, StoreInst)
#define FILC_LLVM_JIT_VAL_SwitchInst(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, SwitchInst)
#define FILC_LLVM_JIT_VAL_User(fname, handle) \
    FILC_LLVM_JIT_VAL_IS_A(fname, handle, a, User)

/* C++ classes with no IsA function of their own, spelled exactly. */

#define FILC_LLVM_JIT_VAL_CallBase(fname, handle) \
    FILC_CHECK(api->LLVMIsACallInst((LLVMValueRef)(handle)) || \
               api->LLVMIsAInvokeInst((LLVMValueRef)(handle)) || \
               api->LLVMIsACallBrInst((LLVMValueRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected a llvm::CallBase (a call, invoke, or callbr).");

#define FILC_LLVM_JIT_VAL_CallBaseOrFuncletPadInst(fname, handle) \
    FILC_CHECK(api->LLVMIsAFuncletPadInst((LLVMValueRef)(handle)) || \
               api->LLVMIsACallInst((LLVMValueRef)(handle)) || \
               api->LLVMIsAInvokeInst((LLVMValueRef)(handle)) || \
               api->LLVMIsACallBrInst((LLVMValueRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected a llvm::CallBase or a llvm::FuncletPadInst.");

/* llvm::GEPOperator matches both getelementptr instructions and
 * getelementptr constant expressions: the ConcreteOperator classof in
 * Operator.h accepts an llvm::Instruction or an llvm::ConstantExpr whose
 * opcode is Instruction::GetElementPtr. */

#define FILC_LLVM_JIT_IS_GEPOPERATOR(handle) \
    (api->LLVMIsAGetElementPtrInst((LLVMValueRef)(handle)) || \
     (api->LLVMIsAConstantExpr((LLVMValueRef)(handle)) && \
      api->LLVMGetConstOpcode((LLVMValueRef)(handle)) == \
      LLVMGetElementPtr))

#define FILC_LLVM_JIT_VAL_GEPOperator(fname, handle) \
    FILC_CHECK(FILC_LLVM_JIT_IS_GEPOPERATOR(handle), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected a llvm::GEPOperator (a getelementptr instruction " \
               "or constant expression).");

#define FILC_LLVM_JIT_VAL_AlignmentOwner(fname, handle) \
    FILC_CHECK(api->LLVMIsAGlobalObject((LLVMValueRef)(handle)) || \
               api->LLVMIsAAllocaInst((LLVMValueRef)(handle)) || \
               api->LLVMIsALoadInst((LLVMValueRef)(handle)) || \
               api->LLVMIsAStoreInst((LLVMValueRef)(handle)) || \
               api->LLVMIsAAtomicRMWInst((LLVMValueRef)(handle)) || \
               api->LLVMIsAAtomicCmpXchgInst((LLVMValueRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected a llvm::GlobalObject, llvm::AllocaInst, " \
               "llvm::LoadInst, llvm::StoreInst, llvm::AtomicRMWInst, or " \
               "llvm::AtomicCmpXchgInst.");

#define FILC_LLVM_JIT_VAL_OrderingOwner(fname, handle) \
    FILC_CHECK(api->LLVMIsAAtomicRMWInst((LLVMValueRef)(handle)) || \
               api->LLVMIsAFenceInst((LLVMValueRef)(handle)) || \
               api->LLVMIsALoadInst((LLVMValueRef)(handle)) || \
               api->LLVMIsAStoreInst((LLVMValueRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected a llvm::AtomicRMWInst, llvm::FenceInst, " \
               "llvm::LoadInst, or llvm::StoreInst.");

#define FILC_LLVM_JIT_VAL_VolatileOwner(fname, handle) \
    FILC_CHECK(api->LLVMIsAAtomicCmpXchgInst((LLVMValueRef)(handle)) || \
               api->LLVMIsAAtomicRMWInst((LLVMValueRef)(handle)) || \
               api->LLVMIsALoadInst((LLVMValueRef)(handle)) || \
               api->LLVMIsAStoreInst((LLVMValueRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected a llvm::AtomicCmpXchgInst, llvm::AtomicRMWInst, " \
               "llvm::LoadInst, or llvm::StoreInst.");

#define FILC_LLVM_JIT_VAL_UnwindOwner(fname, handle) \
    FILC_CHECK(api->LLVMIsACleanupReturnInst((LLVMValueRef)(handle)) || \
               api->LLVMIsACatchSwitchInst((LLVMValueRef)(handle)) || \
               api->LLVMIsAInvokeInst((LLVMValueRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected a llvm::CleanupReturnInst, llvm::CatchSwitchInst, " \
               "or llvm::InvokeInst.");

#define FILC_LLVM_JIT_VAL_PossiblyDisjointInst(fname, handle) \
    FILC_CHECK(api->LLVMIsAInstruction((LLVMValueRef)(handle)) && \
               api->LLVMGetInstructionOpcode((LLVMValueRef)(handle)) == \
               LLVMOr, NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected an `or` instruction (PossiblyDisjointInst).");

#define FILC_LLVM_JIT_VAL_UserOrMDValue(fname, handle) \
    FILC_CHECK(api->LLVMIsAUser((LLVMValueRef)(handle)) || \
               api->ShimValueIsMDNodeOrValueAsMetadata((LLVMValueRef)(handle)), \
               NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected a llvm::User or a metadata-as-value holding an " \
               "llvm::MDNode or llvm::ValueAsMetadata.");

#define FILC_LLVM_JIT_VAL_MDNodeOrValueAsMetadataValue(fname, handle) \
    FILC_CHECK(api->ShimValueIsMDNodeOrValueAsMetadata((LLVMValueRef)(handle)), \
               NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected a metadata-as-value holding an llvm::MDNode or " \
               "llvm::ValueAsMetadata.");

#define FILC_LLVM_JIT_VAL_MDNodeValue(fname, handle) \
    FILC_CHECK(api->ShimValueIsMDNodeValue((LLVMValueRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected a metadata-as-value holding a llvm::MDNode.");

#define FILC_LLVM_JIT_VAL_ExtractMDNodeValue(fname, handle) \
    FILC_CHECK(api->ShimValueExtractMDNodeOK((LLVMValueRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected a metadata-as-value holding an llvm::MDNode or " \
               "llvm::ConstantAsMetadata.");

/* LLVMValueAsMetadata accepts any MetadataAsValue, so the metadata-as-value
 * leg of its check is a value-kind test rather than a subclass test. */
#define FILC_LLVM_JIT_VAL_ConstantOrArgumentOrInstructionOrMDValue( \
    fname, handle) \
    FILC_CHECK(api->LLVMIsAConstant((LLVMValueRef)(handle)) || \
               api->LLVMIsAArgument((LLVMValueRef)(handle)) || \
               api->LLVMIsAInstruction((LLVMValueRef)(handle)) || \
               api->LLVMGetValueKind((LLVMValueRef)(handle)) == \
               LLVMMetadataAsValueValueKind, NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected a llvm::Constant, llvm::Argument, " \
               "llvm::Instruction, or metadata-as-value.");

#define FILC_LLVM_JIT_VAL_InstructionOrGlobalObject(fname, handle) \
    FILC_CHECK(api->LLVMIsAInstruction((LLVMValueRef)(handle)) || \
               api->LLVMIsAGlobalObject((LLVMValueRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected an llvm::Instruction or llvm::GlobalObject.");

#define FILC_LLVM_JIT_VAL_ExtractValueOrInsertValue(fname, handle) \
    FILC_CHECK(api->LLVMIsAExtractValueInst((LLVMValueRef)(handle)) || \
               api->LLVMIsAInsertValueInst((LLVMValueRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected an llvm::ExtractValueInst or llvm::InsertValueInst.");

/* LLVMGetNumIndices dispatches on llvm::GEPOperator first (Core.cpp:3243-3253),
 * and GEPOperator matches getelementptr constant expressions as well as
 * getelementptr instructions (Operator.h's ConcreteOperator classof accepts a
 * ConstantExpr with the LLVMGetElementPtr opcode; llvm-c/Core.h says "NB: This
 * also works on GEP operators").  It then accepts llvm::ExtractValueInst or
 * llvm::InsertValueInst and ends in an llvm_unreachable that aborts even in
 * NDEBUG builds for anything else. */

#define FILC_LLVM_JIT_VAL_NumIndicesOwner(fname, handle) \
    FILC_CHECK(FILC_LLVM_JIT_IS_GEPOPERATOR(handle) || \
               api->LLVMIsAExtractValueInst((LLVMValueRef)(handle)) || \
               api->LLVMIsAInsertValueInst((LLVMValueRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected a getelementptr (instruction or constant " \
               "expression), extractvalue, or insertvalue.");

#define FILC_LLVM_JIT_VAL_DebugLocOwner(fname, handle) \
    FILC_CHECK(api->LLVMIsAInstruction((LLVMValueRef)(handle)) || \
               api->LLVMIsAGlobalVariable((LLVMValueRef)(handle)) || \
               api->LLVMIsAFunction((LLVMValueRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
               ": expected an llvm::Instruction, llvm::GlobalVariable, or " \
               "llvm::Function.");

/* TYPE-kind checks go through LLVMGetTypeKind, which maps the type classes
 * that occur here one to one onto the type kinds. */

#define FILC_LLVM_JIT_VAL_TYPE_KIND(fname, handle, kind_enum, article, class) \
    FILC_CHECK(api->LLVMGetTypeKind((LLVMTypeRef)(handle)) == kind_enum, NULL, \
               "Fil-C LLVM JIT API: wrong LLVM type subclass for " #fname \
               ": expected " #article " llvm::" #class ".");

#define FILC_LLVM_JIT_VAL_FunctionTypeKind(fname, handle) \
    FILC_LLVM_JIT_VAL_TYPE_KIND(fname, handle, LLVMFunctionTypeKind, a, \
                                FunctionType)
#define FILC_LLVM_JIT_VAL_StructTypeKind(fname, handle) \
    FILC_LLVM_JIT_VAL_TYPE_KIND(fname, handle, LLVMStructTypeKind, a, \
                                StructType)
#define FILC_LLVM_JIT_VAL_ArrayTypeKind(fname, handle) \
    FILC_LLVM_JIT_VAL_TYPE_KIND(fname, handle, LLVMArrayTypeKind, an, \
                                ArrayType)
#define FILC_LLVM_JIT_VAL_PointerTypeKind(fname, handle) \
    FILC_LLVM_JIT_VAL_TYPE_KIND(fname, handle, LLVMPointerTypeKind, a, \
                                PointerType)
#define FILC_LLVM_JIT_VAL_IntegerTypeKind(fname, handle) \
    FILC_LLVM_JIT_VAL_TYPE_KIND(fname, handle, LLVMIntegerTypeKind, an, \
                                IntegerType)
#define FILC_LLVM_JIT_VAL_TargetExtTypeKind(fname, handle) \
    FILC_LLVM_JIT_VAL_TYPE_KIND(fname, handle, LLVMTargetExtTypeKind, a, \
                                TargetExtType)

#define FILC_LLVM_JIT_VAL_VectorTypeKind(fname, handle) \
    FILC_CHECK(api->LLVMGetTypeKind((LLVMTypeRef)(handle)) == \
               LLVMVectorTypeKind || \
               api->LLVMGetTypeKind((LLVMTypeRef)(handle)) == \
               LLVMScalableVectorTypeKind, NULL, \
               "Fil-C LLVM JIT API: wrong LLVM type subclass for " #fname \
               ": expected a llvm::VectorType (fixed or scalable).");

#define FILC_LLVM_JIT_VAL_SequenceTypeKind(fname, handle) \
    FILC_CHECK(api->LLVMGetTypeKind((LLVMTypeRef)(handle)) == \
               LLVMArrayTypeKind || \
               api->LLVMGetTypeKind((LLVMTypeRef)(handle)) == \
               LLVMVectorTypeKind || \
               api->LLVMGetTypeKind((LLVMTypeRef)(handle)) == \
               LLVMScalableVectorTypeKind, NULL, \
               "Fil-C LLVM JIT API: wrong LLVM type subclass for " #fname \
               ": expected a llvm::ArrayType or a llvm::VectorType (fixed or " \
               "scalable).");

/* METADATA-kind checks: an LLVMMetadataRef has no C API predicate, so the
 * shim provides one. */

#define FILC_LLVM_JIT_VAL_MDNodeMetadata(fname, handle) \
    FILC_CHECK(api->ShimMetadataIsAMDNode((LLVMMetadataRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM metadata subclass for " #fname \
               ": expected a llvm::MDNode.");

/* ATTRIBUTE-kind checks: the three C predicates partition the attribute
 * space, and the Attribute:: accessors assert on the wrong partition. */

#define FILC_LLVM_JIT_VAL_StringAttribute(fname, handle) \
    FILC_CHECK(api->LLVMIsStringAttribute((LLVMAttributeRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM attribute subclass for " #fname \
               ": expected a string attribute.");

#define FILC_LLVM_JIT_VAL_NotStringAttribute(fname, handle) \
    FILC_CHECK(!api->LLVMIsStringAttribute((LLVMAttributeRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM attribute subclass for " #fname \
               ": expected an enum, integer, or type attribute.");

/* LLVMIsEnumAttribute accepts isEnumAttribute() || isIntAttribute()
 * (Core.cpp), which is exactly the set that LLVMGetEnumAttributeValue
 * accepts: a plain enum attribute reads as value 0, and an integer attribute
 * reads as its payload.  The token is named for the C predicate, not for the
 * accepted set. */
#define FILC_LLVM_JIT_VAL_EnumAttribute(fname, handle) \
    FILC_CHECK(api->LLVMIsEnumAttribute((LLVMAttributeRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM attribute subclass for " #fname \
               ": expected an enum or integer attribute.");

#define FILC_LLVM_JIT_VAL_TypeAttribute(fname, handle) \
    FILC_CHECK(api->LLVMIsTypeAttribute((LLVMAttributeRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM attribute subclass for " #fname \
               ": expected a type attribute.");

/* ARR element checks.  The validate loop invokes them with (wrapper name,
 * element handle, element count) and without a trailing semicolon: the real
 * tokens expand to whole statements, and NONE expands to nothing. */

#define FILC_LLVM_JIT_VAL_ELEM_NONE(fname, handle, count_name)

#define FILC_LLVM_JIT_VAL_ELEM_IS_A(fname, handle, count_name, article, class) \
    FILC_CHECK(api->LLVMIsA##class((LLVMValueRef)(handle)), NULL, \
               "Fil-C LLVM JIT API: wrong LLVM element subclass for " #fname \
               ": expected " #article " llvm::" #class ".");

#define FILC_LLVM_JIT_VAL_ELEM_Constant(fname, handle, count_name) \
    FILC_LLVM_JIT_VAL_ELEM_IS_A(fname, handle, count_name, a, Constant)

/* The ElemNullOK element token applies no subclass check: it exists so that
 * LLVMMDNodeInContext2 can opt out of the null-element check below while
 * still using the ARR token machinery. */

#define FILC_LLVM_JIT_VAL_ELEM_ElemNullOK(fname, handle, count_name)

/* The ElemMDNodeOperand element token mirrors the per-element logic of
 * LLVMMDNodeInContext (Core.cpp): null is mapped to null metadata; a
 * Constant is wrapped as ConstantAsMetadata; a metadata-as-value wrapping a
 * LocalAsMetadata trips the "Unexpected function-local metadata outside of
 * direct argument to call" assert regardless of the element count; any
 * other metadata-as-value is fine; and any other raw value -- which
 * Core.cpp treats as function-local metadata -- is accepted only as the
 * single element.  It expands to statements that continue the validate
 * loop, so it must only ever be expanded from FILC_LLVM_JIT_VALIDATE_ARR. */

#define FILC_LLVM_JIT_VAL_ELEM_ElemMDNodeOperand(fname, handle, count_name) \
    if (!(handle)) { \
        continue; \
    } \
    if (api->LLVMIsAConstant((LLVMValueRef)(handle))) { \
        continue; \
    } \
    if (api->ShimValueIsLocalAsMetadataValue((LLVMValueRef)(handle))) { \
        filc_safety_panic( \
            NULL, \
            "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
            ": function-local metadata (from an argument or instruction) is " \
            "not allowed in a metadata node; pass the value directly as the " \
            "single operand."); \
    } \
    if (api->LLVMGetValueKind((LLVMValueRef)(handle)) == \
        LLVMMetadataAsValueValueKind) { \
        continue; \
    } \
    if ((count_name) != 1) { \
        filc_safety_panic( \
            NULL, \
            "Fil-C LLVM JIT API: wrong LLVM value subclass for " #fname \
            ": only one raw value operand is allowed in a metadata node."); \
    }

/* SEMANTIC CHECKS.
 *
 * The subclass tokens above pin the C++ subclass of one argument at a time.  A
 * second class of LLVM assert is reachable through public wrappers but is
 * keyed on argument semantics: operand types compared against each other,
 * scalar ranges, instruction opcodes, aggregate shapes, and basic-block
 * parents.  Those checks live below: one static helper per rule, built only
 * from api-> primitives, and selected per row with a trailing clause
 * (a bare helper invocation; see FILC_LLVM_JIT_SEM below).  A row's clause
 * expands inside the
 * locked region, after VALIDATE and before the api-> call, so a bad input
 * becomes a Fil-C safety panic before LLVM can assert -- the same
 * panic-before-assert guarantee the subclass tokens give.
 *
 * The helpers stringify the row's wrapper name into the message, using the
 * same convention as the subclass tokens:
 *
 *     Fil-C LLVM JIT API: <what> for <WRAPPER>: <expectation>.
 *
 * Type identity is compared at the raw-handle level: api->LLVMTypeOf returns
 * the uniqued Type*, and LLVM's own asserts compare Type* pointers, so a raw
 * handle compare is exactly as strict as LLVM's.  Never compare filc object
 * pointers here: two wrappers of the same LLVM object are distinct filc
 * objects (handle_of() is the identity).
 *
 * The SEM plumbing: a row may carry one trailing clause, spelled as a bare
 * helper invocation whose first argument is the wrapper name (the same way the
 * VALIDATE tokens are spelled; the invocation must be bare, since a macro name
 * that appears inside another macro's arguments is not expanded).  Rows without
 * a clause compile exactly as before (the empty ##__VA_ARGS__ selects
 * FILC_LLVM_JIT_SEM_NONE).
 */

#define FILC_LLVM_JIT_SEM(fname, ...) \
    FILC_LLVM_JIT_SEM_APPLY(fname, ##__VA_ARGS__, FILC_LLVM_JIT_SEM_NONE)
#define FILC_LLVM_JIT_SEM_APPLY(fname, clause, ...) clause
#define FILC_LLVM_JIT_SEM_NONE

/* Type plumbing. */

static LLVMTypeRef sem_type_of(void* handle)
{
    return (LLVMTypeRef)api->LLVMTypeOf((LLVMValueRef)handle);
}

static unsigned sem_kind(LLVMTypeRef ty)
{
    return api->LLVMGetTypeKind(ty);
}

static bool sem_is_int_ty(LLVMTypeRef ty)
{
    return sem_kind(ty) == LLVMIntegerTypeKind;
}

static bool sem_is_fp_ty(LLVMTypeRef ty)
{
    switch (sem_kind(ty)) {
    case LLVMHalfTypeKind:
    case LLVMBFloatTypeKind:
    case LLVMFloatTypeKind:
    case LLVMDoubleTypeKind:
    case LLVMFP128TypeKind:
    case LLVMX86_FP80TypeKind:
    case LLVMPPC_FP128TypeKind:
        return true;
    default:
        return false;
    }
}

static bool sem_is_ptr_ty(LLVMTypeRef ty)
{
    return sem_kind(ty) == LLVMPointerTypeKind;
}

static bool sem_is_vec_ty(LLVMTypeRef ty)
{
    return sem_kind(ty) == LLVMVectorTypeKind ||
        sem_kind(ty) == LLVMScalableVectorTypeKind;
}

static LLVMTypeRef sem_scalar_ty(LLVMTypeRef ty)
{
    if (!sem_is_vec_ty(ty))
        return ty;
    return api->LLVMGetElementType(ty);
}

static bool sem_is_int_or_int_vec(LLVMTypeRef ty)
{
    return sem_is_int_ty(sem_scalar_ty(ty));
}

static bool sem_is_fp_or_fp_vec(LLVMTypeRef ty)
{
    return sem_is_fp_ty(sem_scalar_ty(ty));
}

static bool sem_is_ptr_or_ptr_vec(LLVMTypeRef ty)
{
    return sem_is_ptr_ty(sem_scalar_ty(ty));
}

static bool sem_is_i1_ty(LLVMTypeRef ty)
{
    return sem_is_int_ty(ty) && api->LLVMGetIntTypeWidth(ty) == 1;
}

static bool sem_is_i1_or_i1_vec_ty(LLVMTypeRef ty)
{
    return sem_is_i1_ty(sem_scalar_ty(ty));
}

/* The scalar bit width of an int or FP type (LLVM's getScalarSizeInBits for
 * the kinds the checks care about; zero for anything else, which the checks
 * never compare).  Vector types yield their element's width. */
static unsigned sem_scalar_bits(LLVMTypeRef ty)
{
    switch (sem_kind(ty)) {
    case LLVMVectorTypeKind:
    case LLVMScalableVectorTypeKind:
        return sem_scalar_bits(sem_scalar_ty(ty));
    case LLVMHalfTypeKind:
    case LLVMBFloatTypeKind:
        return 16;
    case LLVMFloatTypeKind:
        return 32;
    case LLVMDoubleTypeKind:
        return 64;
    case LLVMFP128TypeKind:
    case LLVMPPC_FP128TypeKind:
        return 128;
    case LLVMX86_FP80TypeKind:
        return 80;
    case LLVMIntegerTypeKind:
        return api->LLVMGetIntTypeWidth(ty);
    default:
        return 0;
    }
}

/* The number of elements of a type, counting a scalar as one. */
static unsigned sem_num_elements(LLVMTypeRef ty)
{
    if (!sem_is_vec_ty(ty))
        return 1;
    return api->LLVMGetVectorSize(ty);
}

/* True if a and b are both scalars or are vectors of the same size and
 * scalable-ness.  This mirrors ElementCount ==. */
static bool sem_same_shape(LLVMTypeRef a, LLVMTypeRef b)
{
    bool a_vec;
    bool b_vec;
    a_vec = sem_is_vec_ty(a);
    b_vec = sem_is_vec_ty(b);
    if (a_vec != b_vec)
        return false;
    if (!a_vec)
        return true;
    if (sem_kind(a) != sem_kind(b))
        return false;
    return api->LLVMGetVectorSize(a) == api->LLVMGetVectorSize(b);
}

/* True if a and b have the same primitive size in bits.  Mirrors
 * Type::getPrimitiveSizeInBits(): a scalable vector has a scalable zero size,
 * so any two scalable vectors compare equal. */
static bool sem_same_primitive_size(LLVMTypeRef a, LLVMTypeRef b)
{
    bool a_scalable;
    bool b_scalable;
    a_scalable = sem_kind(a) == LLVMScalableVectorTypeKind;
    b_scalable = sem_kind(b) == LLVMScalableVectorTypeKind;
    if (a_scalable || b_scalable)
        return a_scalable == b_scalable;
    return sem_scalar_bits(a) * sem_num_elements(a) ==
        sem_scalar_bits(b) * sem_num_elements(b);
}

/* The mirror of CastInst::castIsValid (Instructions.cpp), used by the cast
 * wrappers.  Same-type casts are handled by the callers, which implement the
 * IRBuilder's CreateCast short-circuit; the constant paths have no
 * short-circuit and call this directly. */
static bool sem_cast_valid(unsigned op, LLVMTypeRef src, LLVMTypeRef dst)
{
    unsigned src_kind;
    unsigned dst_kind;
    bool src_vec;
    bool dst_vec;
    unsigned src_bits;
    unsigned dst_bits;
    bool same_shape;
    LLVMTypeRef src_scalar;
    LLVMTypeRef dst_scalar;

    /* castIsValid rejects aggregate (struct/array) operands first.  All types
       reachable through the wrappers are first-class, so the only part of the
       first-class rule that matters is the aggregate one. */
    src_kind = sem_kind(src);
    dst_kind = sem_kind(dst);
    if (src_kind == LLVMStructTypeKind || src_kind == LLVMArrayTypeKind ||
        dst_kind == LLVMStructTypeKind || dst_kind == LLVMArrayTypeKind)
        return false;

    src_vec = sem_is_vec_ty(src);
    dst_vec = sem_is_vec_ty(dst);
    src_bits = sem_scalar_bits(src);
    dst_bits = sem_scalar_bits(dst);
    src_scalar = sem_scalar_ty(src);
    dst_scalar = sem_scalar_ty(dst);
    same_shape = sem_same_shape(src, dst);

    switch (op) {
    case LLVMTrunc:
        return sem_is_int_or_int_vec(src) && sem_is_int_or_int_vec(dst) &&
            same_shape && src_bits > dst_bits;
    case LLVMZExt:
    case LLVMSExt:
        return sem_is_int_or_int_vec(src) && sem_is_int_or_int_vec(dst) &&
            same_shape && src_bits < dst_bits;
    case LLVMFPTrunc:
        return sem_is_fp_or_fp_vec(src) && sem_is_fp_or_fp_vec(dst) &&
            same_shape && src_bits > dst_bits;
    case LLVMFPExt:
        return sem_is_fp_or_fp_vec(src) && sem_is_fp_or_fp_vec(dst) &&
            same_shape && src_bits < dst_bits;
    case LLVMUIToFP:
    case LLVMSIToFP:
        return sem_is_int_or_int_vec(src) && sem_is_fp_or_fp_vec(dst) &&
            same_shape;
    case LLVMFPToUI:
    case LLVMFPToSI:
        return sem_is_fp_or_fp_vec(src) && sem_is_int_or_int_vec(dst) &&
            same_shape;
    case LLVMPtrToInt:
        return same_shape && sem_is_ptr_or_ptr_vec(src) &&
            sem_is_int_or_int_vec(dst);
    case LLVMIntToPtr:
        return same_shape && sem_is_int_or_int_vec(src) &&
            sem_is_ptr_or_ptr_vec(dst);
    case LLVMBitCast: {
        bool src_ptr;
        bool dst_ptr;
        src_ptr = sem_is_ptr_ty(src_scalar);
        dst_ptr = sem_is_ptr_ty(dst_scalar);
        if (src_ptr != dst_ptr)
            return false;
        if (!src_ptr)
            return sem_same_primitive_size(src, dst);
        if (api->LLVMGetPointerAddressSpace(src_scalar) !=
            api->LLVMGetPointerAddressSpace(dst_scalar))
            return false;
        if (src_vec && dst_vec)
            return sem_same_shape(src, dst);
        if (src_vec)
            return api->LLVMGetVectorSize(src) == 1;
        if (dst_vec)
            return api->LLVMGetVectorSize(dst) == 1;
        return true;
    }
    case LLVMAddrSpaceCast:
        if (!sem_is_ptr_ty(src_scalar) || !sem_is_ptr_ty(dst_scalar))
            return false;
        if (api->LLVMGetPointerAddressSpace(src_scalar) ==
            api->LLVMGetPointerAddressSpace(dst_scalar))
            return false;
        return same_shape;
    default:
        return false;
    }
}

/* True if the ordering is one of the seven valid llvm::LLVMAtomicOrdering
 * values.  LLVM's mapFromLLVMOrdering (Core.cpp) ends in an llvm_unreachable
 * for anything else.  0xF7 has bits 0, 1, 2, 4, 5, 6, and 7 set: that is the
 * exact value set of the LLVMAtomicOrdering enum in llvm-c/Core.h, which
 * matches llvm::AtomicOrdering in llvm/Support/AtomicOrdering.h (value 3 is
 * the unspecified Consume gap in both) and the cases of mapFromLLVMOrdering.
 * Empirically, orderings 3, 8, and 9 abort with "Invalid LLVMAtomicOrdering
 * value!" in every ordering-taking wrapper, while 0, 1, 2, 4, 5, 6, and 7 all
 * get past the conversion. */
static bool sem_is_ordering(unsigned ordering)
{
    return ordering <= 7 && ((0xF7u >> ordering) & 1);
}

/* True if the binop is one of the valid llvm::LLVMAtomicRMWBinOp values
 * (Xchg..USubSat, i.e. 0..18); mapFromLLVMRMWBinOp ends in an llvm_unreachable
 * for anything else. */
static bool sem_is_atomic_rmw_binop(unsigned binop)
{
    return binop <= LLVMAtomicRMWBinOpUSubSat;
}

/* Message helpers.  Each check reports at most one thing; the wrapper-level
 * checks below compose them so that the panic is deterministic regardless of
 * which LLVM assert would have fired first. */

static void sem_expect_same_type(const char* fname, LLVMTypeRef lhs,
                                 LLVMTypeRef rhs)
{
    FILC_CHECK(lhs == rhs, NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: operands must "
               "have the same type.", fname);
}

static void sem_expect_int_or_int_vec(const char* fname, LLVMTypeRef ty)
{
    FILC_CHECK(sem_is_int_or_int_vec(ty), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: operands must "
               "be integer or vector-of-integer.", fname);
}

static void sem_expect_fp_or_fp_vec(const char* fname, LLVMTypeRef ty)
{
    FILC_CHECK(sem_is_fp_or_fp_vec(ty), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: operands must "
               "be floating-point or vector-of-floating-point.", fname);
}

static void sem_expect_sized(const char* fname, LLVMTypeRef ty)
{
    FILC_CHECK(api->LLVMTypeIsSized(ty), NULL,
               "Fil-C LLVM JIT API: wrong type for %s: the type must be "
               "sized.", fname);
}

static void sem_expect_ptr_operand(const char* fname, LLVMTypeRef ty)
{
    FILC_CHECK(sem_is_ptr_ty(ty), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the pointer "
               "operand must be a pointer.", fname);
}

static void sem_expect_int_operand(const char* fname, LLVMTypeRef ty)
{
    FILC_CHECK(sem_is_int_ty(ty), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the index must "
               "be an integer.", fname);
}

static void sem_expect_ordering(const char* fname, unsigned ordering)
{
    FILC_CHECK(sem_is_ordering(ordering), NULL,
               "Fil-C LLVM JIT API: wrong ordering for %s: the ordering must "
               "be a valid llvm::LLVMAtomicOrdering.", fname);
}

/* Some orderings are legal for the enum but assert per instruction.  An
 * atomicrmw's ordering may be neither NotAtomic ("atomicrmw instructions can
 * only be atomic.", llvm/lib/IR/Instructions.cpp) nor Unordered
 * ("atomicrmw instructions cannot be unordered.", Instructions.cpp), so its
 * set is the enum range minus 0 and 1, i.e. {2, 4, 5, 6, 7}.  The same set
 * holds for a cmpxchg's success ordering (AtomicCmpXchgInst::
 * isValidSuccessOrdering rejects exactly NotAtomic and Unordered,
 * llvm/include/llvm/IR/Instructions.h).  The enum-range check runs first so
 * that an ordering outside the enum keeps reporting the generic message. */
static void sem_expect_rmw_ordering(const char* fname, unsigned ordering)
{
    sem_expect_ordering(fname, ordering);
    FILC_CHECK(ordering != LLVMAtomicOrderingNotAtomic &&
               ordering != LLVMAtomicOrderingUnordered, NULL,
               "Fil-C LLVM JIT API: wrong ordering for %s: the ordering must "
               "be atomic and cannot be unordered.", fname);
}

static void sem_expect_cmpxchg_success_ordering(const char* fname,
                                                unsigned ordering)
{
    sem_expect_ordering(fname, ordering);
    FILC_CHECK(ordering != LLVMAtomicOrderingNotAtomic &&
               ordering != LLVMAtomicOrderingUnordered, NULL,
               "Fil-C LLVM JIT API: wrong ordering for %s: the success "
               "ordering must be atomic and cannot be unordered.", fname);
}

/* A cmpxchg's failure ordering is further restricted: it may not be Release
 * or AcquireRelease either ("invalid CmpXchg failure ordering" via
 * AtomicCmpXchgInst::isValidFailureOrdering, llvm/include/llvm/IR/
 * Instructions.h), so its set is {Monotonic, Acquire, SequentiallyConsistent}. */
static void sem_expect_cmpxchg_failure_ordering(const char* fname,
                                                unsigned ordering)
{
    sem_expect_ordering(fname, ordering);
    FILC_CHECK(ordering == LLVMAtomicOrderingMonotonic ||
               ordering == LLVMAtomicOrderingAcquire ||
               ordering == LLVMAtomicOrderingSequentiallyConsistent, NULL,
               "Fil-C LLVM JIT API: wrong ordering for %s: the failure "
               "ordering must be monotonic, acquire, or sequentially "
               "consistent.", fname);
}

/* LLVM's Value::setNameImpl (Value.cpp) asserts "Cannot assign a name to void
 * values!" when a void-typed value is given a non-empty name.  The wrappers
 * below name the value that they produce or touch, so a name is only allowed
 * when that value is not void. */

static void sem_expect_nameable(const char* fname, bool is_void)
{
    FILC_CHECK(!is_void, NULL,
               "Fil-C LLVM JIT API: wrong name for %s: cannot assign a name to "
               "void values.", fname);
}

/* The call-like builders name the instruction that they insert, and the
 * instruction's type is the return type of the given function type.  The
 * fences have no type parameter and are always void. */
static void sem_check_nameable_result_impl(const char* fname,
                                           void* fn_ty_or_null,
                                           const char* name)
{
    bool is_void;
    if (!name || !*name)
        return;
    if (fn_ty_or_null)
        is_void = sem_kind(api->LLVMGetReturnType(
            (LLVMTypeRef)fn_ty_or_null)) == LLVMVoidTypeKind;
    else
        is_void = true;
    sem_expect_nameable(fname, is_void);
}

/* The same check, but for wrappers whose type parameter is the produced
   value's type directly (a landing pad's result type is its type parameter,
   not a function type). */

static void sem_check_nameable_ty_impl(const char* fname, void* ty,
                                       const char* name)
{
    bool is_void;
    if (!name || !*name)
        return;
    is_void = sem_kind((LLVMTypeRef)ty) == LLVMVoidTypeKind;
    sem_expect_nameable(fname, is_void);
}

/* The name setters name an existing value. */

static void sem_check_nameable_value_impl(const char* fname, void* val_handle,
                                          const char* name)
{
    bool is_void;
    if (!name || !*name)
        return;
    is_void = sem_kind(sem_type_of(val_handle)) == LLVMVoidTypeKind;
    sem_expect_nameable(fname, is_void);
}

/* Same, but the name is a pointer/length pair and need not be
 * NUL-terminated.  Value::setNameImpl (Value.cpp) also asserts "Null bytes
 * are not allowed in names"; only the length-delimited setters can receive an
 * embedded NUL, since the C-string helpers cannot produce one (they require a
 * NUL terminator inside the range). */

static void sem_check_nameable_value_len_impl(const char* fname,
                                              void* val_handle,
                                              const char* name,
                                              size_t name_len)
{
    bool is_void;
    size_t i;
    if (!name || !name_len)
        return;
    is_void = sem_kind(sem_type_of(val_handle)) == LLVMVoidTypeKind;
    sem_expect_nameable(fname, is_void);
    for (i = 0; i < name_len; ++i) {
        FILC_CHECK(name[i] != '\0', NULL,
                   "Fil-C LLVM JIT API: wrong name for %s: names cannot "
                   "contain null bytes.", fname);
    }
}

static void sem_expect_binop(const char* fname, unsigned binop)
{
    FILC_CHECK(sem_is_atomic_rmw_binop(binop), NULL,
               "Fil-C LLVM JIT API: wrong binop for %s: the binop must be a "
               "valid llvm::LLVMAtomicRMWBinOp.", fname);
}

static void* sem_fn_of_builder(void* builder_handle)
{
    return api->LLVMGetBasicBlockParent(
        (LLVMBasicBlockRef)api->LLVMGetInsertBlock((LLVMBuilderRef)builder_handle));
}

static void sem_expect_insert_position(const char* fname, void* builder_handle)
{
    FILC_CHECK(api->LLVMGetInsertBlock((LLVMBuilderRef)builder_handle), NULL,
               "Fil-C LLVM JIT API: wrong builder for %s: the builder has no "
               "insert position.", fname);
}

/* The basic block must belong to the function that the builder is building
 * into.  A block of another function is only observable when CompileModule
 * clones the module, and the clone asserts before any verifier runs. */
static void* sem_parent_of_bb(void* bb_handle)
{
    return api->LLVMGetBasicBlockParent((LLVMBasicBlockRef)bb_handle);
}

static void* sem_parent_of_inst(void* inst_handle)
{
    return api->LLVMGetBasicBlockParent(
        (LLVMBasicBlockRef)api->LLVMGetInstructionParent((LLVMValueRef)inst_handle));
}

static void sem_expect_bb_of_builder(const char* fname, void* builder_handle,
                                     void* bb_handle)
{
    FILC_CHECK(sem_fn_of_builder(builder_handle) == sem_parent_of_bb(bb_handle),
               NULL,
               "Fil-C LLVM JIT API: wrong basic block for %s: the basic block "
               "belongs to a different function.", fname);
}

static void sem_expect_bb_of_inst(const char* fname, void* inst_handle,
                                  void* bb_handle)
{
    FILC_CHECK(sem_parent_of_inst(inst_handle) == sem_parent_of_bb(bb_handle),
               NULL,
               "Fil-C LLVM JIT API: wrong basic block for %s: the basic block "
               "belongs to a different function.", fname);
}

/* The opposite direction of the checks above: the wrappers that move an
 * existing basic block into a function's block list require that the block
 * has NO parent.  BasicBlock::insertInto (BasicBlock.cpp) asserts
 * "!Parent && \"Already has a parent\"" and the list traits assert
 * "!V->getParent() && \"Value already in a container!!\""
 * (SymbolTableListTraitsImpl.h) for a block that already belongs to a
 * function. */

static void sem_check_unparented_bb_impl(const char* fname, void* bb_handle)
{
    FILC_CHECK(!sem_parent_of_bb(bb_handle), NULL,
               "Fil-C LLVM JIT API: wrong basic block for %s: the basic block "
               "already belongs to a function.", fname);
}

/* The insert-after-insert-block wrapper (Core.cpp) asserts
 * "current insertion point is invalid!" when the builder has no insert
 * block, before it can even look at the block. */

static void sem_check_insert_bb_impl(const char* fname, void* builder_handle,
                                     void* bb_handle)
{
    sem_expect_insert_position(fname, builder_handle);
    sem_check_unparented_bb_impl(fname, bb_handle);
}

/* A short name for a type, for the messages that describe a type
 * combination. */

static const char* sem_type_name(LLVMTypeRef ty)

{
    switch (sem_kind(ty)) {
    case LLVMVoidTypeKind:
        return "void";
    case LLVMHalfTypeKind:
    case LLVMBFloatTypeKind:
    case LLVMFloatTypeKind:
    case LLVMDoubleTypeKind:
    case LLVMFP128TypeKind:
    case LLVMX86_FP80TypeKind:
    case LLVMPPC_FP128TypeKind:
        return "floating-point";
    case LLVMLabelTypeKind:
        return "label";
    case LLVMIntegerTypeKind:
        return "integer";
    case LLVMFunctionTypeKind:
        return "function";
    case LLVMStructTypeKind:
        return "struct";
    case LLVMArrayTypeKind:
        return "array";
    case LLVMPointerTypeKind:
        return "pointer";
    case LLVMVectorTypeKind:
        return "vector";
    case LLVMMetadataTypeKind:
        return "metadata";
    case LLVMTokenTypeKind:
        return "token";
    case LLVMScalableVectorTypeKind:
        return "scalable vector";
    case LLVMX86_AMXTypeKind:
        return "x86_amx";
    case LLVMTargetExtTypeKind:
        return "target extension";
    default:
        return "unknown";
    }
}

/* The bit count that CastInst::getCastOpcode's size compares use:
 * Type::getPrimitiveSizeInBits, which is the scalar size times the element
 * count for vectors, 8192 for x86_amx, and zero for types with no primitive
 * size (pointers, labels, token, metadata, aggregates, function). */

static unsigned sem_cast_opcode_bits(LLVMTypeRef ty)
{
    if (sem_kind(ty) == LLVMX86_AMXTypeKind)
        return 8192;
    return sem_scalar_bits(ty) * sem_num_elements(ty);
}

/* The mirror of CastInst::getCastOpcode's dispatch (Instructions.cpp).  The
 * picker ends in an assert or an llvm_unreachable for every type combination
 * that no cast opcode covers: "Only first class types are castable!",
 * "Casting vector to integer of different width", "Casting from a value that
 * is not first-class type", "Casting pointer or non-first class to float",
 * "Casting vector to floating point of different width", "Illegal cast to
 * vector (wrong type or size)", "Casting pointer to other than pointer or
 * int", "Casting to type that is not first-class", and the "Invalid size
 * request on a scalable vector" fatal error of the unsigned primitive-size
 * reads.  Its accepted set is wider than castIsValid's (which the cast
 * wrappers mirror through sem_cast_valid): the picker returns BitCast for
 * same-type aggregates and for scalar and vector types of the same total
 * size, and it never applies castIsValid's aggregate rule, so this is its
 * own mirror rather than a reuse of sem_cast_valid. */

static bool sem_cast_opcode_ok(LLVMTypeRef src, LLVMTypeRef dst)
{
    unsigned src_kind;
    unsigned dst_kind;
    unsigned src_bits;
    unsigned dst_bits;

    src_kind = sem_kind(src);
    dst_kind = sem_kind(dst);

    /* First-class means not a function and not void (Type::isFirstClassType,
     * Type.h).  The assert fires before the same-type short circuit, so a
     * void-to-void or function-to-function cast is rejected. */
    if (src_kind == LLVMVoidTypeKind || src_kind == LLVMFunctionTypeKind ||
        dst_kind == LLVMVoidTypeKind || dst_kind == LLVMFunctionTypeKind)
        return false;

    if (src == dst)
        return true;

    /* A vector-to-vector cast with the same element count is decided on the
       element types. */
    if (sem_is_vec_ty(src) && sem_is_vec_ty(dst) && sem_same_shape(src, dst)) {
        src = sem_scalar_ty(src);
        dst = sem_scalar_ty(dst);
        src_kind = sem_kind(src);
        dst_kind = sem_kind(dst);
    }

    /* The picker reads both primitive sizes into `unsigned` before it
       dispatches; a scalable type that the demotion above did not strip dies
       in that read with the "Invalid size request on a scalable vector"
       fatal error. */
    if (src_kind == LLVMScalableVectorTypeKind ||
        dst_kind == LLVMScalableVectorTypeKind)
        return false;

    src_bits = sem_cast_opcode_bits(src);
    dst_bits = sem_cast_opcode_bits(dst);

    if (dst_kind == LLVMIntegerTypeKind) {
        if (sem_is_int_ty(src))
            return true;
        if (sem_is_fp_ty(src))
            return true;
        if (sem_is_vec_ty(src))
            return src_bits == dst_bits;
        return sem_is_ptr_ty(src);
    }
    if (sem_is_fp_ty(dst)) {
        if (sem_is_int_ty(src) || sem_is_fp_ty(src))
            return true;
        if (sem_is_vec_ty(src))
            return src_bits == dst_bits;
        return false;
    }
    if (sem_is_vec_ty(dst))
        return src_bits == dst_bits;
    if (dst_kind == LLVMPointerTypeKind) {
        if (sem_is_ptr_ty(src))
            return true;
        return sem_is_int_ty(src);
    }
    return false;
}

static void sem_check_cast_opcode_impl(const char* fname, void* val,
                                       void* dest_ty)
{
    LLVMTypeRef src;
    LLVMTypeRef dst;
    src = sem_type_of(val);
    dst = (LLVMTypeRef)dest_ty;
    FILC_CHECK(sem_cast_opcode_ok(src, dst), NULL,
               "Fil-C LLVM JIT API: wrong cast operand types for %s: there is "
               "no cast opcode from %s to %s.", fname, sem_type_name(src),
               sem_type_name(dst));
}

/* PointerType::get stores the address space in Type's 24-bit SubclassData
 * field (Type::setSubclassData asserts "Subclass data too large for field",
 * Type.h, for anything larger).  The global and ifunc creators reach the
 * same call through the GlobalValue constructors, so all of them share this
 * bound: 16777216 = 2^24. */

static void sem_check_address_space_impl(const char* fname, unsigned addr_space)
{
    FILC_CHECK(addr_space < 0x1000000u, NULL,
               "Fil-C LLVM JIT API: wrong address space for %s: the address "
               "space must be less than 16777216.", fname);
}

/* CallInst::setTailCallKind packs the kind into a two-bit bitfield
 * (Bitfields.h asserts "value is too big"), so only the four
 * llvm::LLVMTailCallKind values fit: the C header's enum tops out at
 * LLVMTailCallKindNoTail (3). */

static void sem_check_tail_call_kind_impl(const char* fname, unsigned kind)
{
    FILC_CHECK(kind <= LLVMTailCallKindNoTail, NULL,
               "Fil-C LLVM JIT API: wrong tail call kind for %s: the tail "
               "call kind must be a valid llvm::LLVMTailCallKind.", fname);
}

/* Attribute::getWithAlignment (Attributes.cpp) and Argument::addAttr reach
 * Align's constructor (Alignment.h), which asserts "Value must not be 0" and
 * "Alignment is not a power of 2".  Unlike LLVMSetAlignment, there is no
 * global-object zero allowance here: the alignment is the attribute's
 * value. */

static void sem_check_param_alignment_impl(const char* fname, unsigned align)
{
    FILC_CHECK(align != 0 && (align & (align - 1)) == 0, NULL,
               "Fil-C LLVM JIT API: wrong alignment for %s: the alignment "
               "must be a non-zero power of two.", fname);
}

/* Instruction::insertInto (Instruction.cpp) asserts "Expected detached
 * instruction" when the instruction already has a parent.  The assert only
 * runs when the builder has an insert position: IRBuilderDefaultInserter::
 * InsertHelper (IRBuilder.h) skips the insert, and the assert with it, for a
 * builder without one, renaming the instruction instead. */

static void sem_check_detached_impl(const char* fname, void* builder_handle,
                                    void* inst_handle)
{
    if (!api->LLVMGetInsertBlock((LLVMBuilderRef)builder_handle))
        return;
    FILC_CHECK(!api->LLVMGetInstructionParent((LLVMValueRef)inst_handle), NULL,
               "Fil-C LLVM JIT API: wrong instruction for %s: the instruction "
               "must be detached (have no parent).", fname);
}

/* A builder without an insert position creates detached instructions, which
 * no function owns, so there is nothing to compare a linked block against
 * and nothing for CompileModule to remap.  A builder with an insert position
 * must only link blocks of its own function: a cross-function reference is
 * only observable when CompileModule clones the module, and the clone
 * asserts ("Referenced value not in value map!", ValueMapper.cpp) before any
 * verifier runs. */

static void sem_check_catchswitch_unwind_impl(const char* fname,
                                              void* builder_handle,
                                              void* bb_handle)
{
    if (!api->LLVMGetInsertBlock((LLVMBuilderRef)builder_handle))
        return;
    sem_expect_bb_of_builder(fname, builder_handle, bb_handle);
}

/* GlobalAlias::setAliasee (Globals.cpp) asserts "Alias and aliasee types
 * should match!" when the aliasee's type differs from the alias's own type.
 * The alias's type is uniqued, so the raw-handle compare is exactly as strict
 * as LLVM's Type* compare. */

static void sem_check_alias_aliasee_impl(const char* fname, void* alias_handle,
                                         void* aliasee_handle)
{
    FILC_CHECK(sem_type_of(aliasee_handle) == sem_type_of(alias_handle), NULL,
               "Fil-C LLVM JIT API: wrong aliasee type for %s: the aliasee "
               "type must match the alias type.", fname);
}

/* LLVMAddAlias2 builds the alias with PointerType::get(Context, AddrSpace)
 * (GlobalValue's constructor ignores the pointee type), and then setAliasee
 * makes the same compare as above.  So with opaque pointers the aliasee
 * matches iff its type is a pointer type in the given address space. */

static void sem_check_alias_value_ty_impl(const char* fname,
                                          void* aliasee_handle,
                                          unsigned addr_space)
{
    LLVMTypeRef ty;
    ty = sem_type_of(aliasee_handle);
    FILC_CHECK(sem_is_ptr_ty(ty) &&
               api->LLVMGetPointerAddressSpace(ty) == addr_space, NULL,
               "Fil-C LLVM JIT API: wrong aliasee type for %s: the aliasee "
               "type must be a pointer with address space %u.", fname,
               addr_space);
}

/* Wrapper-level checks.  The FILC_LLVM_JIT_SEM clauses invoke these; each
 * stringifies the wrapper name for the message and fetches raw handles from
 * the descriptor objects. */

static void sem_check_int_binop_impl(const char* fname, void* lhs, void* rhs)
{
    LLVMTypeRef lhs_ty;
    LLVMTypeRef rhs_ty;
    lhs_ty = sem_type_of(lhs);
    rhs_ty = sem_type_of(rhs);
    sem_expect_same_type(fname, lhs_ty, rhs_ty);
    sem_expect_int_or_int_vec(fname, lhs_ty);
}

static void sem_check_fp_binop_impl(const char* fname, void* lhs, void* rhs)
{
    LLVMTypeRef lhs_ty;
    LLVMTypeRef rhs_ty;
    lhs_ty = sem_type_of(lhs);
    rhs_ty = sem_type_of(rhs);
    sem_expect_same_type(fname, lhs_ty, rhs_ty);
    sem_expect_fp_or_fp_vec(fname, lhs_ty);
}

static void sem_check_icmp_impl(const char* fname, unsigned pred, void* lhs,
                                void* rhs)
{
    LLVMTypeRef lhs_ty;
    LLVMTypeRef rhs_ty;
    FILC_CHECK(pred >= LLVMIntEQ && pred <= LLVMIntSLE, NULL,
               "Fil-C LLVM JIT API: wrong predicate for %s: predicate %u is "
               "not a valid llvm::LLVMIntPredicate.", fname, pred);
    lhs_ty = sem_type_of(lhs);
    rhs_ty = sem_type_of(rhs);
    sem_expect_same_type(fname, lhs_ty, rhs_ty);
    FILC_CHECK(sem_is_int_or_int_vec(lhs_ty) || sem_is_ptr_or_ptr_vec(lhs_ty),
               NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: operands must "
               "be integer, pointer, or vectors of those.", fname);
}

static void sem_check_fcmp_impl(const char* fname, unsigned pred, void* lhs,
                                void* rhs)
{
    LLVMTypeRef lhs_ty;
    LLVMTypeRef rhs_ty;
    FILC_CHECK(pred <= LLVMRealPredicateTrue, NULL,
               "Fil-C LLVM JIT API: wrong predicate for %s: predicate %u is "
               "not a valid llvm::LLVMRealPredicate.", fname, pred);
    lhs_ty = sem_type_of(lhs);
    rhs_ty = sem_type_of(rhs);
    sem_expect_same_type(fname, lhs_ty, rhs_ty);
    sem_expect_fp_or_fp_vec(fname, lhs_ty);
}

static void sem_check_select_impl(const char* fname, void* cond, void* then_val,
                                  void* else_val)
{
    LLVMTypeRef cond_ty;
    cond_ty = sem_type_of(cond);
    FILC_CHECK(sem_is_i1_or_i1_vec_ty(cond_ty), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the condition "
               "must be an i1 or a vector of i1.", fname);
    sem_expect_same_type(fname, sem_type_of(then_val),
                         sem_type_of(else_val));
}

/* CallInst::init and InvokeInst::init (Instructions.cpp) assert
 * "Calling a function with bad signature!"/"Invoking a function with bad
 * signature" unless the argument count is exactly the number of fixed
 * parameters, or the function type is vararg and there are more.  So fewer
 * args than fixed parameters is bad for vararg and non-vararg types alike,
 * and too many args is only bad for a non-vararg function type. */

static void sem_check_call_sig_impl(const char* fname, void* ty,
                                    unsigned num_args)
{
    LLVMTypeRef fn_ty;
    bool var_arg;
    unsigned num_params;
    fn_ty = (LLVMTypeRef)ty;
    var_arg = api->LLVMIsFunctionVarArg(fn_ty) != 0;
    num_params = api->LLVMCountParamTypes(fn_ty);
    FILC_CHECK(num_args == num_params || (var_arg && num_args > num_params),
               NULL,
               "Fil-C LLVM JIT API: wrong argument count for %s: the function "
               "type takes %u%s argument(s) but %u were given.",
               fname, num_params, var_arg ? " or more" : "", num_args);
}

static void sem_check_load_impl(const char* fname, void* ty, void* ptr)
{
    sem_expect_ptr_operand(fname, sem_type_of(ptr));
    sem_expect_sized(fname, (LLVMTypeRef)ty);
}

static void sem_check_array_alloc_impl(const char* fname, void* ty, void* val)
{
    sem_expect_sized(fname, (LLVMTypeRef)ty);
    FILC_CHECK(sem_is_int_or_int_vec(sem_type_of(val)), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the array size "
               "must be an integer.", fname);
}

static void sem_check_atomic_rmw_impl(const char* fname, unsigned binop,
                                      void* ptr, unsigned ordering)
{
    sem_expect_binop(fname, binop);
    sem_expect_ptr_operand(fname, sem_type_of(ptr));
    sem_expect_rmw_ordering(fname, ordering);
}

static void sem_check_atomic_cmpxchg_impl(const char* fname, void* ptr,
                                          void* cmp, void* new_val,
                                          unsigned success_ordering,
                                          unsigned failure_ordering)
{
    sem_expect_ptr_operand(fname, sem_type_of(ptr));
    sem_expect_cmpxchg_success_ordering(fname, success_ordering);
    sem_expect_cmpxchg_failure_ordering(fname, failure_ordering);
    /* AtomicCmpXchgInst::Init asserts "Cmp type and NewVal type must be
     * same!" (llvm/lib/IR/Instructions.cpp). */
    sem_expect_same_type(fname, sem_type_of(cmp), sem_type_of(new_val));
}

/* LLVMSetOrdering dispatches on the instruction kind (Core.cpp): a load,
 * store, or fence takes any enum-range ordering (LoadInst/StoreInst/FenceInst
 * have no ordering asserts in this LLVM; fence with NotAtomic was probed
 * clean), while an atomicrmw's setter asserts "atomicrmw instructions can
 * only be atomic." and "atomicrmw instructions cannot be unordered."
 * (llvm/include/llvm/IR/Instructions.h).  The row's OrderingOwner subclass
 * check has already rejected cmpxchg (Core.cpp would cast<StoreInst> it). */
static void sem_check_set_ordering_impl(const char* fname, void* inst,
                                        unsigned ordering)
{
    if (api->LLVMIsAAtomicRMWInst((LLVMValueRef)inst))
        sem_expect_rmw_ordering(fname, ordering);
    else
        sem_expect_ordering(fname, ordering);
}

/* Constant::getAggregateElement asserts "Must be an aggregate/vector
 * constant" (llvm/lib/IR/Constants.cpp) for a constant whose type is neither
 * an aggregate (struct or array) nor a vector; isVectorTy covers fixed and
 * scalable vectors alike. */
static void sem_check_aggregate_element_impl(const char* fname, void* value)
{
    unsigned kind;
    kind = sem_kind(sem_type_of(value));
    FILC_CHECK(kind == LLVMStructTypeKind || kind == LLVMArrayTypeKind ||
               kind == LLVMVectorTypeKind ||
               kind == LLVMScalableVectorTypeKind, NULL,
               "Fil-C LLVM JIT API: wrong type for %s: the constant's type "
               "must be an aggregate or vector type.", fname);
}

/* ConstantExpr::getGetElementPtr asserts "Element type is unsupported!"
 * (llvm/lib/IR/Constants.cpp) when the source element type (the type
 * parameter, which is what LLVM checks) is scalable, and then "GEP indices
 * invalid!" when GetElementPtrInst::getIndexedType rejects the indices.
 * getIndexedType only examines the indices after the first one, so a
 * single-index getelementptr accepts an index of any type (and the
 * gep-lie success controls rely on that); from the second index on, each
 * index indexes a composite and must be an integer. */
static void sem_check_const_gep_impl(const char* fname, void* ty,
                                     void* const* indices, unsigned num_indices)
{
    unsigned i;
    FILC_CHECK(sem_kind((LLVMTypeRef)ty) != LLVMScalableVectorTypeKind, NULL,
               "Fil-C LLVM JIT API: wrong type for %s: the source element "
               "type must not be a scalable vector type.", fname);
    for (i = 1; i < num_indices; ++i)
        sem_expect_int_operand(fname, sem_type_of((void*)indices[i]));
}

static void sem_check_va_arg_impl(const char* fname, void* list, void* ty)
{
    sem_expect_ptr_operand(fname, sem_type_of(list));
    FILC_CHECK(sem_kind((LLVMTypeRef)ty) != LLVMVoidTypeKind, NULL,
               "Fil-C LLVM JIT API: wrong type for %s: the type must not be "
               "void.", fname);
}

static void sem_check_cond_br_impl(const char* fname, void* builder, void* cond,
                                   void* then_bb, void* else_bb)
{
    LLVMTypeRef cond_ty;
    cond_ty = sem_type_of(cond);
    FILC_CHECK(sem_is_i1_ty(cond_ty), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the condition "
               "must be an i1.", fname);
    sem_expect_insert_position(fname, builder);
    sem_expect_bb_of_builder(fname, builder, then_bb);
    sem_expect_bb_of_builder(fname, builder, else_bb);
}

static void sem_check_cast_impl(const char* fname, unsigned op, void* val,
                                void* dest_ty)
{
    LLVMTypeRef src;
    LLVMTypeRef dst;
    src = sem_type_of(val);
    dst = (LLVMTypeRef)dest_ty;
    FILC_CHECK(src == dst || sem_cast_valid(op, src, dst), NULL,
               "Fil-C LLVM JIT API: invalid cast for %s: the cast from the "
               "operand type to the destination type is not a valid "
               "llvm::CastInst cast.", fname);
}

static void sem_check_build_cast_impl(const char* fname, unsigned op, void* val,
                                      void* dest_ty)
{
    switch (op) {
    case LLVMTrunc:
    case LLVMZExt:
    case LLVMSExt:
    case LLVMFPToUI:
    case LLVMFPToSI:
    case LLVMUIToFP:
    case LLVMSIToFP:
    case LLVMFPTrunc:
    case LLVMFPExt:
    case LLVMPtrToInt:
    case LLVMIntToPtr:
    case LLVMBitCast:
    case LLVMAddrSpaceCast:
        break;
    default:
        filc_safety_panic(
            NULL,
            "Fil-C LLVM JIT API: wrong opcode for %s: the opcode must be a "
            "cast opcode.", fname);
    }
    sem_check_cast_impl(fname, op, val, dest_ty);
}

static void sem_check_int_cast_impl(const char* fname, void* val, void* dest_ty)
{
    LLVMTypeRef src;
    LLVMTypeRef dst;
    src = sem_type_of(val);
    dst = (LLVMTypeRef)dest_ty;
    FILC_CHECK((src == dst ||
                (sem_is_int_or_int_vec(src) && sem_is_int_or_int_vec(dst) &&
                 sem_same_shape(src, dst))), NULL,
               "Fil-C LLVM JIT API: invalid cast for %s: the cast from the "
               "operand type to the destination type is not a valid integer "
               "cast.", fname);
}

/* CastInst::CreatePointerCast: the source must be a pointer (or pointer
 * vector), the destination must be a pointer or integer of the same shape, or
 * the types must already match. */
static void sem_check_pointer_cast_impl(const char* fname, void* val,
                                        void* dest_ty)
{
    LLVMTypeRef src;
    LLVMTypeRef dst;
    src = sem_type_of(val);
    dst = (LLVMTypeRef)dest_ty;
    FILC_CHECK(src == dst ||
               (sem_is_ptr_or_ptr_vec(src) &&
                (sem_is_ptr_or_ptr_vec(dst) || sem_is_int_or_int_vec(dst)) &&
                sem_same_shape(src, dst)), NULL,
               "Fil-C LLVM JIT API: invalid cast for %s: the cast from the "
               "operand type to the destination type is not a valid pointer "
               "cast.", fname);
}

/* IRBuilder::CreateFPCast (IRBuilder.h): the same-type case is a legal no-op
 * (CreateCast returns the operand unchanged), and otherwise the builder picks
 * FPTrunc or FPExt by comparing scalar sizes and CastInst::Create asserts
 * castIsValid (Instructions.cpp), so both types must be floating-point (or
 * vectors of the same shape) with different scalar sizes. */
static void sem_check_fp_cast_impl(const char* fname, void* val,
                                   void* dest_ty)
{
    LLVMTypeRef src;
    LLVMTypeRef dst;
    src = sem_type_of(val);
    dst = (LLVMTypeRef)dest_ty;
    FILC_CHECK(src == dst ||
               (sem_is_fp_or_fp_vec(src) && sem_is_fp_or_fp_vec(dst) &&
                sem_same_shape(src, dst) &&
                sem_scalar_bits(src) != sem_scalar_bits(dst)), NULL,
               "Fil-C LLVM JIT API: invalid cast for %s: the cast from the "
               "operand type to the destination type is not a valid "
               "floating-point cast.", fname);
}

/* ConstantExpr::getPointerCast has no same-type short-circuit: the source must
 * be a pointer (or pointer vector) and the destination a pointer or integer. */
static void sem_check_const_pointer_cast_impl(const char* fname, void* val,
                                              void* dest_ty)
{
    LLVMTypeRef src;
    LLVMTypeRef dst;
    src = sem_type_of(val);
    dst = (LLVMTypeRef)dest_ty;
    FILC_CHECK(sem_is_ptr_or_ptr_vec(src) &&
               (sem_is_ptr_or_ptr_vec(dst) || sem_is_int_or_int_vec(dst)), NULL,
               "Fil-C LLVM JIT API: invalid cast for %s: the cast from the "
               "operand type to the destination type is not a valid pointer "
               "cast.", fname);
}

static void sem_check_or_bit_cast_impl(const char* fname, unsigned op,
                                       void* val, void* dest_ty)
{
    LLVMTypeRef src;
    LLVMTypeRef dst;
    src = sem_type_of(val);
    dst = (LLVMTypeRef)dest_ty;
    FILC_CHECK(src == dst || sem_cast_valid(op, src, dst) ||
               sem_cast_valid(LLVMBitCast, src, dst), NULL,
               "Fil-C LLVM JIT API: invalid cast for %s: the cast from the "
               "operand type to the destination type is not a valid cast.",
               fname);
}

/* ExtractValueInst::getIndexedType: the aggregate must be a struct or array
 * and the single index in range; anything else leaves the index type null and
 * trips the checkGEPType assert. */
static void sem_check_extract_value_impl(const char* fname, void* agg,
                                         unsigned index)
{
    LLVMTypeRef ty;
    unsigned num_elements;
    bool ok;
    ty = sem_type_of(agg);
    switch (sem_kind(ty)) {
    case LLVMStructTypeKind:
        num_elements = api->LLVMCountStructElementTypes(ty);
        ok = index < num_elements;
        break;
    case LLVMArrayTypeKind:
        num_elements = api->LLVMGetArrayLength(ty);
        ok = index < num_elements;
        break;
    default:
        ok = false;
        break;
    }
    FILC_CHECK(ok, NULL,
               "Fil-C LLVM JIT API: wrong aggregate for %s: the aggregate must "
               "be a struct or array, and the index must be in range.", fname);
}

static void sem_check_aggregate_ret_impl(const char* fname, void* builder)
{
    void* fn;
    LLVMTypeRef fn_ty;
    LLVMTypeRef ret_ty;
    unsigned kind;
    sem_expect_insert_position(fname, builder);
    fn = sem_fn_of_builder(builder);
    FILC_CHECK(fn, NULL,
               "Fil-C LLVM JIT API: wrong basic block for %s: the builder is "
               "not in a function.", fname);
    fn_ty = (LLVMTypeRef)api->LLVMGlobalGetValueType((LLVMValueRef)fn);
    ret_ty = api->LLVMGetReturnType(fn_ty);
    kind = sem_kind(ret_ty);
    FILC_CHECK(kind == LLVMStructTypeKind || kind == LLVMArrayTypeKind, NULL,
               "Fil-C LLVM JIT API: wrong return type for %s: the function "
               "must return an aggregate type.", fname);
}

static void sem_check_extract_element_impl(const char* fname, void* vec,
                                           void* index)
{
    LLVMTypeRef vec_ty;
    vec_ty = sem_type_of(vec);
    FILC_CHECK(sem_is_vec_ty(vec_ty), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the vector "
               "operand must be a vector.", fname);
    sem_expect_int_operand(fname, sem_type_of(index));
}

static void sem_check_insert_element_impl(const char* fname, void* vec,
                                          void* elt, void* index)
{
    LLVMTypeRef vec_ty;
    vec_ty = sem_type_of(vec);
    FILC_CHECK(sem_is_vec_ty(vec_ty), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the vector "
               "operand must be a vector.", fname);
    sem_expect_same_type(fname, sem_type_of(elt),
                         api->LLVMGetElementType(vec_ty));
    sem_expect_int_operand(fname, sem_type_of(index));
}

static void sem_check_shuffle_vector_impl(const char* fname, void* v1, void* v2,
                                          void* mask)
{
    LLVMTypeRef v1_ty;
    LLVMTypeRef mask_ty;
    v1_ty = sem_type_of(v1);
    FILC_CHECK(sem_is_vec_ty(v1_ty), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the first two "
               "operands must be vectors of the same type and the mask must be "
               "an integer vector.", fname);
    FILC_CHECK(v1_ty == sem_type_of(v2), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the first two "
               "operands must be vectors of the same type and the mask must be "
               "an integer vector.", fname);
    /* IRBuilder::CreateShuffleVector folds the mask through
       ShuffleVectorInst::getShuffleMask, which requires a constant integer
       vector of either flavor. */
    mask_ty = sem_type_of(mask);
    FILC_CHECK(sem_is_vec_ty(mask_ty) &&
               api->LLVMIsAConstant((LLVMValueRef)mask) &&
               sem_is_int_ty(api->LLVMGetElementType(mask_ty)), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the first two "
               "operands must be vectors of the same type and the mask must be "
               "an integer vector.", fname);
    /* CreateShuffleVector reads the mask through
       ShuffleVectorInst::getShuffleMask and then asserts
       ShuffleVectorInst::isValidOperands (Instructions.cpp): every element
       must be poison or less than twice the source vector's element count,
       and a scalable source needs a uniform mask of all zeros or all
       poison. */
    FILC_CHECK(api->ShimShuffleMaskOK((LLVMValueRef)v1,
                                      (LLVMValueRef)mask) != 0, NULL,
               "Fil-C LLVM JIT API: wrong mask for %s: each mask element must "
               "be poison or less than twice the source vector's element "
               "count, and a scalable source requires a uniform mask.",
               fname);
}

static void sem_check_const_unary_int_impl(const char* fname, void* val)
{
    FILC_CHECK(sem_is_int_or_int_vec(sem_type_of(val)), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the operand "
               "must be integer or vector-of-integer.", fname);
}

/* ConstantArray::get asserts that every element constant has the array's
 * element type (Core.cpp builds ArrayType::get(element_ty, length) from the
 * element type directly, so the element type itself is what the elements are
 * compared against). */
static void sem_check_const_array_impl(const char* fname, void* elem_ty,
                                       void* const* vals, unsigned long long count)
{
    LLVMTypeRef want;
    unsigned long long i;
    want = (LLVMTypeRef)elem_ty;
    for (i = 0; i < count; ++i) {
        FILC_CHECK(sem_type_of((void*)vals[i]) == want, NULL,
                   "Fil-C LLVM JIT API: wrong element type for %s: element "
                   "initializers must have the array's element type.", fname);
    }
}

static void sem_check_const_named_struct_impl(const char* fname, void* struct_ty,
                                              void* const* vals,
                                              unsigned long long count)
{
    LLVMTypeRef ty;
    unsigned long long i;
    ty = (LLVMTypeRef)struct_ty;
    FILC_CHECK(api->LLVMCountStructElementTypes(ty) == count, NULL,
               "Fil-C LLVM JIT API: wrong element count for %s: the count must "
               "match the struct type's field count.", fname);
    for (i = 0; i < count; ++i) {
        FILC_CHECK(sem_type_of((void*)vals[i]) ==
                   api->LLVMStructGetTypeAtIndex(ty, (unsigned)i), NULL,
                   "Fil-C LLVM JIT API: wrong element type for %s: element "
                   "initializers must have the struct's field types.", fname);
    }
}

static void sem_check_nonempty_impl(const char* fname, unsigned count)
{
    FILC_CHECK(count != 0, NULL,
               "Fil-C LLVM JIT API: wrong element count for %s: the vector "
               "must have at least one element.", fname);
}

static void sem_check_fp_ty_impl(const char* fname, void* ty)
{
    FILC_CHECK(sem_is_fp_ty((LLVMTypeRef)ty), NULL,
               "Fil-C LLVM JIT API: wrong type for %s: the type must be a "
               "floating-point type.", fname);
}

/* Constant::getNullValue (Constants.cpp) llvm_unreachables ("Cannot create a
 * null constant of that type!") on label and metadata types, and dispatches
 * target extension types to ConstantTargetNone::get, which asserts the
 * zeroinit property ("Target extension type not allowed to have a
 * zeroinitializer"). */
static void sem_check_const_null_impl(const char* fname, void* ty)
{
    unsigned kind;
    kind = sem_kind((LLVMTypeRef)ty);
    FILC_CHECK(kind != LLVMVoidTypeKind && kind != LLVMFunctionTypeKind &&
               kind != LLVMLabelTypeKind && kind != LLVMMetadataTypeKind, NULL,
               "Fil-C LLVM JIT API: wrong type for %s: the type must not be "
               "void, a function, a label, or a metadata type.", fname);
    FILC_CHECK(api->ShimTargetExtNullOK((LLVMTypeRef)ty) != 0, NULL,
               "Fil-C LLVM JIT API: wrong type for %s: a target extension "
               "type must allow a zeroinitializer.", fname);
}

static void sem_check_const_all_ones_impl(const char* fname, void* ty)
{
    LLVMTypeRef the_ty;
    the_ty = (LLVMTypeRef)ty;
    FILC_CHECK(sem_is_int_or_int_vec(the_ty) || sem_is_fp_or_fp_vec(the_ty),
               NULL,
               "Fil-C LLVM JIT API: wrong type for %s: the type must be "
               "integer, floating-point, or a vector of those.", fname);
}

/* ConstantPtrAuth::ConstantPtrAuth (Constants.cpp) asserts that the pointer
 * and the address discriminator are pointers, that the key is a 32-bit
 * integer, and that the discriminator is a 64-bit integer. */
static void sem_check_const_ptr_auth_impl(const char* fname, void* ptr,
                                          void* key, void* disc,
                                          void* addr_disc)
{
    LLVMTypeRef ptr_ty;
    LLVMTypeRef key_ty;
    LLVMTypeRef disc_ty;
    LLVMTypeRef addr_disc_ty;
    ptr_ty = sem_type_of(ptr);
    key_ty = sem_type_of(key);
    disc_ty = sem_type_of(disc);
    addr_disc_ty = sem_type_of(addr_disc);
    FILC_CHECK(sem_kind(ptr_ty) == LLVMPointerTypeKind &&
               sem_kind(addr_disc_ty) == LLVMPointerTypeKind &&
               sem_kind(key_ty) == LLVMIntegerTypeKind &&
               api->LLVMGetIntTypeWidth(key_ty) == 32 &&
               sem_kind(disc_ty) == LLVMIntegerTypeKind &&
               api->LLVMGetIntTypeWidth(disc_ty) == 64, NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the pointer "
               "and the address discriminator must be pointers, the key must "
               "be a 32-bit integer, and the discriminator must be a 64-bit "
               "integer.", fname);
}

static void sem_check_const_extract_element_impl(const char* fname, void* vec,
                                                 void* index)
{
    FILC_CHECK(sem_is_vec_ty(sem_type_of(vec)), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the vector "
               "operand must be a vector.", fname);
    sem_expect_int_operand(fname, sem_type_of(index));
}

static void sem_check_const_insert_element_impl(const char* fname, void* vec,
                                                void* elt, void* index)
{
    LLVMTypeRef vec_ty;
    vec_ty = sem_type_of(vec);
    FILC_CHECK(sem_is_vec_ty(vec_ty), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the vector "
               "operand must be a vector.", fname);
    sem_expect_same_type(fname, sem_type_of(elt),
                         api->LLVMGetElementType(vec_ty));
    sem_expect_int_operand(fname, sem_type_of(index));
}

static void sem_check_const_shuffle_vector_impl(const char* fname, void* v1,
                                                void* v2, void* mask)
{
    LLVMTypeRef v1_ty;
    LLVMTypeRef mask_ty;
    v1_ty = sem_type_of(v1);
    FILC_CHECK(sem_is_vec_ty(v1_ty), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the first two "
               "operands must be vectors of the same type and the mask must be "
               "an integer vector.", fname);
    FILC_CHECK(v1_ty == sem_type_of(v2), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the first two "
               "operands must be vectors of the same type and the mask must be "
               "an integer vector.", fname);
    mask_ty = sem_type_of(mask);
    FILC_CHECK(sem_is_vec_ty(mask_ty) &&
               sem_is_int_ty(api->LLVMGetElementType(mask_ty)), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the first two "
               "operands must be vectors of the same type and the mask must be "
               "an integer vector.", fname);
    /* ConstantExpr::getShuffleVector reads the mask through
       ShuffleVectorInst::getShuffleMask and then asserts
       ShuffleVectorInst::isValidOperands (Instructions.cpp): every element
       must be poison or less than twice the source vector's element count,
       and a scalable source needs a uniform mask of all zeros or all
       poison. */
    FILC_CHECK(api->ShimShuffleMaskOK((LLVMValueRef)v1,
                                      (LLVMValueRef)mask) != 0, NULL,
               "Fil-C LLVM JIT API: wrong mask for %s: each mask element must "
               "be poison or less than twice the source vector's element "
               "count, and a scalable source requires a uniform mask.",
               fname);
}

static void sem_check_array_elem_ty_impl(const char* fname, void* elem_ty)
{
    unsigned kind;
    kind = sem_kind((LLVMTypeRef)elem_ty);
    FILC_CHECK(kind != LLVMVoidTypeKind && kind != LLVMFunctionTypeKind &&
               kind != LLVMLabelTypeKind && kind != LLVMTokenTypeKind &&
               kind != LLVMMetadataTypeKind, NULL,
               "Fil-C LLVM JIT API: wrong element type for %s: the element "
               "type must be a valid array element type.", fname);
}

static void sem_check_vector_elem_ty_impl(const char* fname, void* elem_ty)
{
    LLVMTypeRef ty;
    ty = (LLVMTypeRef)elem_ty;
    FILC_CHECK(sem_is_int_ty(ty) || sem_is_fp_ty(ty) || sem_is_ptr_ty(ty),
               NULL,
               "Fil-C LLVM JIT API: wrong element type for %s: the element "
               "type must be an integer, floating-point, or pointer type.",
               fname);
}

/* FunctionType's constructor asserts isValidReturnType (Type.cpp): every type
 * is a valid function return type except function, label, and metadata
 * (FunctionType::isValidReturnType). */
static void sem_check_fn_ret_ty_impl(const char* fname, void* ret_ty)
{
    unsigned kind;
    kind = sem_kind((LLVMTypeRef)ret_ty);
    FILC_CHECK(kind != LLVMFunctionTypeKind && kind != LLVMLabelTypeKind &&
               kind != LLVMMetadataTypeKind, NULL,
               "Fil-C LLVM JIT API: wrong return type for %s: the return type "
               "must not be a function, label, or metadata type.", fname);
}

/* FunctionType's constructor asserts isValidArgumentType (Type.cpp) for
 * every parameter: a valid argument type is any first-class type, which in
 * this LLVM is everything except function and void (Type::isFirstClassType).
 * Label and metadata parameters are legal (probed clean: LLVM builds
 * function types taking label and metadata parameters just fine). */
static void sem_check_fn_params_impl(const char* fname, LLVMTypeRef const* params,
                                     unsigned count)
{
    unsigned i;
    for (i = 0; i < count; ++i) {
        FILC_CHECK(sem_kind(params[i]) != LLVMVoidTypeKind &&
                       sem_kind(params[i]) != LLVMFunctionTypeKind, NULL,
                   "Fil-C LLVM JIT API: wrong parameter type for %s: parameter "
                   "types must not be void or a function type.", fname);
    }
}

static void sem_check_global_ty_impl(const char* fname, void* ty)
{
    unsigned kind;
    kind = sem_kind((LLVMTypeRef)ty);
    FILC_CHECK(kind != LLVMVoidTypeKind && kind != LLVMFunctionTypeKind &&
               kind != LLVMLabelTypeKind && kind != LLVMTokenTypeKind &&
               kind != LLVMMetadataTypeKind, NULL,
               "Fil-C LLVM JIT API: wrong type for %s: the type must be a "
               "valid global variable type.", fname);
}

static void sem_check_set_initializer_impl(const char* fname, void* global,
                                           void* val)
{
    LLVMTypeRef want;
    want = (LLVMTypeRef)api->LLVMGlobalGetValueType((LLVMValueRef)global);
    sem_expect_same_type(fname, sem_type_of(val), want);
}

static void sem_check_operand_index_impl(const char* fname, void* user,
                                         unsigned index)
{
    FILC_CHECK((int)index < api->LLVMGetNumOperands((LLVMValueRef)user), NULL,
               "Fil-C LLVM JIT API: wrong operand index for %s: the index must "
               "be less than the number of operands.", fname);
}

static void sem_check_incoming_index_impl(const char* fname, void* phi,
                                          unsigned index)
{
    FILC_CHECK(index < api->LLVMCountIncoming((LLVMValueRef)phi), NULL,
               "Fil-C LLVM JIT API: wrong incoming index for %s: the index "
               "must be less than the number of incoming values.", fname);
}

static void sem_check_successor_index_impl(const char* fname, void* term,
                                           unsigned index)
{
    FILC_CHECK(index < api->LLVMGetNumSuccessors((LLVMValueRef)term), NULL,
               "Fil-C LLVM JIT API: wrong successor index for %s: the index "
               "must be less than the number of successors.", fname);
}

static void sem_check_mask_index_impl(const char* fname, void* shuffle,
                                      unsigned index)
{
    FILC_CHECK(index < api->LLVMGetNumMaskElements((LLVMValueRef)shuffle), NULL,
               "Fil-C LLVM JIT API: wrong mask index for %s: the index must be "
               "less than the number of mask elements.", fname);
}

static void sem_check_arg_operand_index_impl(const char* fname, void* funclet,
                                             unsigned index)
{
    FILC_CHECK(index < api->LLVMGetNumArgOperands((LLVMValueRef)funclet), NULL,
               "Fil-C LLVM JIT API: wrong argument index for %s: the index "
               "must be less than the number of argument operands.", fname);
}

/* LLVMGetParam indexes straight into the function's argument array
 * (Function::getArg), so an out-of-range index reads wild memory. */
static void sem_check_param_index_impl(const char* fname, void* fn,
                                       unsigned index)
{
    FILC_CHECK(index < api->LLVMCountParams((LLVMValueRef)fn), NULL,
               "Fil-C LLVM JIT API: wrong parameter index for %s: the index "
               "must be less than the number of parameters.", fname);
}

/* LLVMGetCallBrIndirectDest indexes the callbr's operand array raw
 * (CallBrInst::getIndirectDest is *(&Op<-1>() - getNumIndirectDests() + i)),
 * so an out-of-range index reads wild memory. */
static void sem_check_callbr_indirect_dest_index_impl(const char* fname,
                                                      void* callbr,
                                                      unsigned index)
{
    FILC_CHECK(
        index < api->LLVMGetCallBrNumIndirectDests((LLVMValueRef)callbr), NULL,
        "Fil-C LLVM JIT API: wrong indirect destination index for %s: the "
        "index must be less than the number of indirect destinations.",
        fname);
}

/* MDNode::replaceOperandWith reads the operand through getOperand, which
 * asserts I < getNumOperands ("Out of range", Metadata.h). */
static void sem_check_md_operand_index_impl(const char* fname, void* mdnode,
                                            unsigned index)
{
    FILC_CHECK(index < api->LLVMGetMDNodeNumOperands((LLVMValueRef)mdnode),
               NULL,
               "Fil-C LLVM JIT API: wrong operand index for %s: the index "
               "must be less than the number of operands.", fname);
}

/* TargetExtType's parameter accessors index their arrays raw:
 * getTypeParameter goes through Type::getContainedType, which asserts
 * "Index out of range!" (Type.h), and getIntParameter indexes an
 * ArrayRef-like pointer whose out-of-range read is undefined.  The caller
 * fetches the count from the companion NumParams getter. */
static void sem_check_target_ext_param_index_impl(const char* fname,
                                                  unsigned index,
                                                  unsigned count)
{
    FILC_CHECK(index < count, NULL,
               "Fil-C LLVM JIT API: wrong index for %s: the index must be "
               "less than the target extension type's parameter count.",
               fname);
}

/* The metadata-entries objects record their array's length at creation (the
 * getters index the raw array, whose length LLVM does not expose), so an
 * out-of-range index reads past LLVM-owned memory and returns garbage or a
 * wild metadata reference. */
static void sem_check_entries_index_impl(const char* fname, void* entries_obj,
                                         unsigned index)
{
    FILC_CHECK(index < ((filc_llvm_jit_entries_object*)entries_obj)->count,
               NULL,
               "Fil-C LLVM JIT API: wrong index for %s: the index must be "
               "less than the number of entries.", fname);
}

/* The calling convention is stored in a 10-bit field: Function::setCallingConv
 * asserts !(ID & ~CallingConv::MaxID) ("Unsupported calling convention",
 * Function.h) and CallBase::setCallingConv packs the id into the same-width
 * bitfield, which asserts "value is too big" (Bitfields.h).  1023 is
 * CallingConv::MaxID. */
static void sem_check_call_conv_impl(const char* fname, unsigned call_conv)
{
    FILC_CHECK(!(call_conv & ~1023u), NULL,
               "Fil-C LLVM JIT API: wrong calling convention for %s: the "
               "calling convention must fit in llvm::CallingConv::MaxID.",
               fname);
}

/* StructType::getElementType asserts N < NumContainedTys ("Element number out
 * of range!", DerivedTypes.h).  An opaque struct has no contained types, so
 * every index is out of range for one. */
static void sem_check_struct_element_index_impl(const char* fname,
                                                void* struct_ty, unsigned index)
{
    FILC_CHECK(index < api->LLVMCountStructElementTypes((LLVMTypeRef)struct_ty),
               NULL,
               "Fil-C LLVM JIT API: wrong index for %s: the index must be less "
               "than the struct's element count.", fname);
}

static void sem_check_clause_index_impl(const char* fname, void* landing_pad,
                                        unsigned index)
{
    FILC_CHECK(index < api->LLVMGetNumClauses((LLVMValueRef)landing_pad), NULL,
               "Fil-C LLVM JIT API: wrong clause index for %s: the index must "
               "be less than the number of clauses.", fname);
}

static void sem_check_alignment_impl(const char* fname, void* value,
                                     unsigned bytes)
{
    bool ok;
    ok = bytes && !(bytes & (bytes - 1));
    if (!ok && !bytes)
        ok = api->LLVMIsAGlobalObject((LLVMValueRef)value) != 0;
    FILC_CHECK(ok, NULL,
               "Fil-C LLVM JIT API: wrong alignment for %s: the alignment must "
               "be a nonzero power of two, or zero for a global object.",
               fname);
}

/* The memcpy/memmove/memset builders convert their alignment to a
 * MaybeAlign, which asserts "Alignment is neither 0 nor a power of 2"
 * (Alignment.h) for anything else; zero means "unspecified". */
static void sem_check_mem_align_impl(const char* fname, unsigned align)
{
    FILC_CHECK(align == 0 || (align & (align - 1)) == 0, NULL,
               "Fil-C LLVM JIT API: wrong alignment for %s: the alignment "
               "must be zero or a power of two.", fname);
}

static void sem_check_nsw_impl(const char* fname, void* inst)
{
    unsigned opcode;
    opcode = api->LLVMGetInstructionOpcode((LLVMValueRef)inst);
    FILC_CHECK(opcode == LLVMAdd || opcode == LLVMSub || opcode == LLVMMul ||
               opcode == LLVMShl ||
               api->LLVMIsATruncInst((LLVMValueRef)inst) != 0, NULL,
               "Fil-C LLVM JIT API: wrong opcode for %s: the instruction must "
               "be an add, sub, mul, shl, or trunc.", fname);
}

static void sem_check_exact_impl(const char* fname, void* inst)
{
    unsigned opcode;
    opcode = api->LLVMGetInstructionOpcode((LLVMValueRef)inst);
    FILC_CHECK(opcode == LLVMUDiv || opcode == LLVMSDiv ||
               opcode == LLVMLShr || opcode == LLVMAShr, NULL,
               "Fil-C LLVM JIT API: wrong opcode for %s: the instruction must "
               "be a udiv, sdiv, lshr, or ashr.", fname);
}

static void sem_check_nneg_impl(const char* fname, void* inst)
{
    unsigned opcode;
    opcode = api->LLVMGetInstructionOpcode((LLVMValueRef)inst);
    FILC_CHECK(opcode == LLVMZExt || opcode == LLVMUIToFP, NULL,
               "Fil-C LLVM JIT API: wrong opcode for %s: the instruction must "
               "be a zext or uitofp.", fname);
}

/* The FPMathOperator set, from Operator.h's classof: the FP math and FP cast
 * opcodes, FCmp, and PHI/Select/Call when the result is an FP type. */
static void sem_check_fpmath_impl(const char* fname, void* inst)
{
    unsigned opcode;
    opcode = api->LLVMGetInstructionOpcode((LLVMValueRef)inst);
    FILC_CHECK(opcode == LLVMFNeg || opcode == LLVMFAdd ||
               opcode == LLVMFSub || opcode == LLVMFMul ||
               opcode == LLVMFDiv || opcode == LLVMFRem ||
               opcode == LLVMFPTrunc || opcode == LLVMFPExt ||
               opcode == LLVMFCmp ||
               ((opcode == LLVMPHI || opcode == LLVMSelect ||
                 opcode == LLVMCall) &&
                sem_is_fp_or_fp_vec(sem_type_of(inst))), NULL,
               "Fil-C LLVM JIT API: wrong opcode for %s: the instruction must "
               "be a floating-point operator.", fname);
}

static void sem_check_rauw_impl(const char* fname, void* old_val, void* new_val)
{
    FILC_CHECK(old_val != new_val, NULL,
               "Fil-C LLVM JIT API: wrong operands for %s: the two values must "
               "be different.", fname);
}

static void sem_check_int_unop_impl(const char* fname, void* val)
{
    sem_expect_int_or_int_vec(fname, sem_type_of(val));
}

static void sem_check_fp_unop_impl(const char* fname, void* val)
{
    sem_expect_fp_or_fp_vec(fname, sem_type_of(val));
}

/* ConstantExpr::getPtrToInt and getIntToPtr: the source and destination must
 * have the same shape, and the pointer/integer sides must line up. */
static void sem_check_const_ptr_to_int_impl(const char* fname, void* val,
                                            void* dest_ty)
{
    LLVMTypeRef src;
    LLVMTypeRef dst;
    src = sem_type_of(val);
    dst = (LLVMTypeRef)dest_ty;
    FILC_CHECK(sem_is_ptr_or_ptr_vec(src) && sem_is_int_or_int_vec(dst) &&
               sem_same_shape(src, dst), NULL,
               "Fil-C LLVM JIT API: invalid cast for %s: the cast from the "
               "operand type to the destination type is not a valid "
               "llvm::CastInst cast.", fname);
}

static void sem_check_const_int_to_ptr_impl(const char* fname, void* val,
                                            void* dest_ty)
{
    LLVMTypeRef src;
    LLVMTypeRef dst;
    src = sem_type_of(val);
    dst = (LLVMTypeRef)dest_ty;
    FILC_CHECK(sem_is_int_or_int_vec(src) && sem_is_ptr_or_ptr_vec(dst) &&
               sem_same_shape(src, dst), NULL,
               "Fil-C LLVM JIT API: invalid cast for %s: the cast from the "
               "operand type to the destination type is not a valid "
               "llvm::CastInst cast.", fname);
}

static void sem_check_invoke_impl(const char* fname, void* builder, void* ty,
                                  unsigned num_args, void* then_bb,
                                  void* catch_bb)
{
    sem_check_call_sig_impl(fname, ty, num_args);
    sem_expect_insert_position(fname, builder);
    sem_expect_bb_of_builder(fname, builder, then_bb);
    sem_expect_bb_of_builder(fname, builder, catch_bb);
}

static void sem_check_phi_ty_impl(const char* fname, void* ty)
{
    unsigned kind;
    kind = sem_kind((LLVMTypeRef)ty);
    FILC_CHECK(kind != LLVMVoidTypeKind && kind != LLVMFunctionTypeKind, NULL,
               "Fil-C LLVM JIT API: wrong type for %s: the type must not be "
               "void or a function type.", fname);
    /* PHINode's constructor (Instructions.h) asserts "PHI nodes cannot have
       token type!".  Label types were probed fine. */
    FILC_CHECK(kind != LLVMTokenTypeKind, NULL,
               "Fil-C LLVM JIT API: wrong type for %s: the type must not be "
               "the token type.", fname);
}

static void sem_check_ptr_diff2_impl(const char* fname, void* lhs, void* rhs)
{
    sem_expect_ptr_operand(fname, sem_type_of(lhs));
    sem_expect_ptr_operand(fname, sem_type_of(rhs));
    /* IRBuilderBase::CreatePtrDiff asserts that the two pointer operands
       have the same type ("Pointer subtraction operand types must match!",
       IRBuilder.cpp); with opaque pointers the reachable mismatch is the
       address space.  The element type is deliberately not checked:
       CreatePtrDiff only uses it to size the divisor. */
    FILC_CHECK(sem_type_of(lhs) == sem_type_of(rhs), NULL,
               "Fil-C LLVM JIT API: wrong operand types for %s: the pointer "
               "operands must have the same type.", fname);
}

/* The LLVM C API's intrinsic entry points map the id through
 * llvm_map_to_intrinsic_id, which asserts ID < Intrinsic::num_intrinsics
 * ("Intrinsic ID out of range", Core.cpp). */
static void sem_check_intrinsic_id_impl(const char* fname, unsigned id)
{
    FILC_CHECK(api->ShimIntrinsicIDOK(id) != 0, NULL,
               "Fil-C LLVM JIT API: wrong intrinsic id for %s: the intrinsic "
               "id must be less than the number of known intrinsics.", fname);
}

/* The intrinsic parameter-types array must agree with the id's overloadedness
 * AND with the id's type table.  The Tys array is fed straight into LLVM's
 * name and type machinery:
 *
 *   - getIntrinsicNameImpl asserts "This version of getName is for overloaded
 *     intrinsics only" (Intrinsics.cpp) for a non-overloaded id with Tys; the
 *     exact rule below (LLVMIntrinsicIsOverloaded(id) == (count != 0)) guards
 *     that for the declaration and copy-name wrappers, while the type getter
 *     is only ever fed the one-way form -- overloaded implies non-empty --
 *     because a non-overloaded id ignores the Tys it is given (they only
 *     matter for mangling and for the overloaded decode).
 *   - Intrinsic::getType (which LLVMGetIntrinsicDeclaration and
 *     LLVMIntrinsicGetType both run) walks the id's IIT descriptors while
 *     indexing the Tys array and constructing types, and asserts for wrong
 *     counts ("Invalid index!", ArrayRef.h), wrong kinds ("invalid return
 *     type for function" / "Not a valid type for function argument!",
 *     Type.cpp:345/351 via the FunctionType constructor that finishes the
 *     decode; the argument transforms' cast<>s; the VectorType constructor's
 *     element/count asserts), and every shape the argument transforms reject.
 *     The ShimIntrinsicTysOK predicate mirrors that decode exactly, so the
 *     tys-ok clause below pins all of it.
 *   - The declaration and copy-name wrappers additionally mangle every Tys
 *     element into the intrinsic's name (getMangledTypeStr,
 *     Intrinsics.cpp), which llvm_unreachables ("Unhandled type") for label
 *     and token types; the tys-mangle clause rejects exactly those.  (The
 *     mangling path is otherwise graceful: it handles void, function, and
 *     metadata types, and for wrong counts it would only build a garbage
 *     name -- but the decode mirroring above already rejects every
 *     wrong-count input these wrappers can be given.)
 *
 * LLVMIntrinsicIsOverloaded is graceful for in-range ids, which the
 * intrinsic-id clause has already guaranteed. */
static void sem_check_intrinsic_tys_exact_impl(const char* fname, unsigned id,
                                               size_t count)
{
    FILC_CHECK((api->LLVMIntrinsicIsOverloaded(id) != 0) == (count != 0), NULL,
               "Fil-C LLVM JIT API: wrong argument types for %s: the parameter "
               "types must be passed exactly for overloaded intrinsics and not "
               "at all for non-overloaded ones.", fname);
}

static void sem_check_intrinsic_tys_overloaded_impl(const char* fname,
                                                    unsigned id, size_t count)
{
    FILC_CHECK(api->LLVMIntrinsicIsOverloaded(id) == 0 || count != 0, NULL,
               "Fil-C LLVM JIT API: wrong argument types for %s: the parameter "
               "types must be passed for overloaded intrinsics.", fname);
}

/* The type-table decode (see the comment above): false when LLVM's own
 * decode of the id's IIT descriptors against this Tys array would trip one
 * of its assertions. */
static void sem_check_intrinsic_tys_ok_impl(const char* fname, unsigned id,
                                            LLVMTypeRef const* tys,
                                            size_t count)
{
    FILC_CHECK(api->ShimIntrinsicTysOK(id, (LLVMTypeRef*)(tys), count) != 0,
               NULL,
               "Fil-C LLVM JIT API: wrong argument types for %s: the parameter "
               "types must decode as the intrinsic's type parameters.",
               fname);
}

/* The name mangling (see the comment above): getMangledTypeStr
 * llvm_unreachables for label and token types.  Every other type kind is
 * mangled gracefully. */
static void sem_check_intrinsic_tys_mangle_ok_impl(const char* fname,
                                                   LLVMTypeRef const* tys,
                                                   size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i) {
        unsigned kind;
        kind = sem_kind(tys[i]);
        FILC_CHECK(kind != LLVMLabelTypeKind && kind != LLVMTokenTypeKind,
                   NULL,
                   "Fil-C LLVM JIT API: wrong argument types for %s: the "
                   "parameter types must be types the intrinsic name can be "
                   "mangled with.", fname);
    }
}

/* LLVMGetAtomicSyncScopeID and LLVMSetAtomicSyncScopeID assert
 * I->isAtomic() ("Expected an atomic instruction", Core.cpp) after the C
 * API's own Instruction downcast. */
static void sem_check_atomic_inst_impl(const char* fname, void* inst)
{
    FILC_CHECK(api->ShimInstructionIsAtomic((LLVMValueRef)inst) != 0, NULL,
               "Fil-C LLVM JIT API: wrong instruction for %s: the "
               "instruction must be atomic.", fname);
}

/* PHINode::addIncoming asserts that every incoming value has the PHI's type,
 * and a cross-function incoming block is only observable when CompileModule
 * clones the module (the clone asserts before any verifier runs). */
static void sem_check_add_incoming_impl(const char* fname, void* phi,
                                        void* const* values,
                                        void* const* blocks, unsigned count)
{
    LLVMTypeRef phi_ty;
    unsigned i;
    phi_ty = sem_type_of(phi);
    for (i = 0; i < count; ++i) {
        sem_expect_same_type(fname, sem_type_of((void*)values[i]), phi_ty);
        sem_expect_bb_of_inst(fname, phi, (void*)blocks[i]);
    }
}

static void sem_check_module_flag_behavior_impl(const char* fname,
                                                unsigned behavior)
{
    FILC_CHECK(behavior <= LLVMModuleFlagBehaviorAppendUnique, NULL,
               "Fil-C LLVM JIT API: wrong behavior for %s: the behavior must "
               "be a valid llvm::LLVMModuleFlagBehavior.", fname);
}

/* Row-level clause macros.  Each takes the wrapper name and the descriptor
 * names, stringifies the name, and fetches raw handles from the descriptor
 * objects. */

#define sem_int_binop(fname, lhs, rhs) \
    sem_check_int_binop_impl(#fname, (lhs##_obj)->handle, (rhs##_obj)->handle);
#define sem_int_unop(fname, val) \
    sem_check_int_unop_impl(#fname, (val##_obj)->handle);
#define sem_fp_unop(fname, val) \
    sem_check_fp_unop_impl(#fname, (val##_obj)->handle);
#define sem_fp_binop(fname, lhs, rhs) \
    sem_check_fp_binop_impl(#fname, (lhs##_obj)->handle, (rhs##_obj)->handle);
#define sem_binop(fname, opcode, lhs, rhs) \
    do { \
        switch (opcode) { \
        case LLVMAdd: \
        case LLVMSub: \
        case LLVMMul: \
        case LLVMShl: \
        case LLVMLShr: \
        case LLVMAShr: \
        case LLVMAnd: \
        case LLVMOr: \
        case LLVMXor: \
        case LLVMUDiv: \
        case LLVMSDiv: \
        case LLVMURem: \
        case LLVMSRem: \
            sem_check_int_binop_impl(#fname, (lhs##_obj)->handle, \
                                     (rhs##_obj)->handle); \
            break; \
        case LLVMFAdd: \
        case LLVMFSub: \
        case LLVMFMul: \
        case LLVMFDiv: \
        case LLVMFRem: \
            sem_check_fp_binop_impl(#fname, (lhs##_obj)->handle, \
                                    (rhs##_obj)->handle); \
            break; \
        default: \
            break; \
        } \
    } while (0);
#define sem_icmp(fname, pred, lhs, rhs) \
    sem_check_icmp_impl(#fname, (pred), (lhs##_obj)->handle, \
                        (rhs##_obj)->handle);
#define sem_fcmp(fname, pred, lhs, rhs) \
    sem_check_fcmp_impl(#fname, (pred), (lhs##_obj)->handle, \
                        (rhs##_obj)->handle);
#define sem_select(fname, cond, then_val, else_val) \
    sem_check_select_impl(#fname, (cond##_obj)->handle, \
                          (then_val##_obj)->handle, (else_val##_obj)->handle);
#define sem_call_sig(fname, ty, num_args) \
    sem_check_call_sig_impl(#fname, (ty##_obj)->handle, (num_args));
#define sem_sized(fname, ty) \
    sem_expect_sized(#fname, (LLVMTypeRef)(ty##_obj)->handle);
#define sem_load(fname, ty, ptr) \
    sem_check_load_impl(#fname, (ty##_obj)->handle, (ptr##_obj)->handle);
/* StoreInst's constructor sizes the value's type through DataLayout, which
 * asserts Ty->isSized() ("Cannot getTypeInfo() on a type that is unsized!",
 * DataLayout.cpp); the load wrappers make the same demand on their type
 * operand via the same helper. */
#define sem_store_value(fname, val) \
    sem_expect_sized(#fname, sem_type_of((val##_obj)->handle));
#define sem_array_alloc(fname, ty, val) \
    sem_check_array_alloc_impl(#fname, (ty##_obj)->handle, \
                               (val##_obj)->handle);
#define sem_ptr_operand(fname, val) \
    sem_expect_ptr_operand(#fname, sem_type_of((val##_obj)->handle));
#define sem_atomic_rmw(fname, bin_op, ptr, ordering) \
    sem_check_atomic_rmw_impl(#fname, (bin_op), (ptr##_obj)->handle, \
                              (ordering));
#define sem_atomic_cmpxchg(fname, ptr, cmp, new, success_ordering, \
                           failure_ordering) \
    sem_check_atomic_cmpxchg_impl(#fname, (ptr##_obj)->handle, \
                                  (cmp##_obj)->handle, \
                                  (new##_obj)->handle, (success_ordering), \
                                  (failure_ordering));
#define sem_ordering(fname, ordering) \
    sem_expect_ordering(#fname, (ordering));
#define sem_cmpxchg_success_ordering(fname, ordering) \
    sem_expect_cmpxchg_success_ordering(#fname, (ordering));
#define sem_cmpxchg_failure_ordering(fname, ordering) \
    sem_expect_cmpxchg_failure_ordering(#fname, (ordering));
#define sem_owner_ordering(fname, inst, ordering) \
    sem_check_set_ordering_impl(#fname, (inst##_obj)->handle, (ordering));
#define sem_nameable_result(fname, ty, name) \
    sem_check_nameable_result_impl(#fname, (ty##_obj)->handle, (name##_obj));
#define sem_unnamed(fname, name) \
    sem_check_nameable_result_impl(#fname, NULL, (name##_obj));
#define sem_nameable_value(fname, val, name) \
    sem_check_nameable_value_impl(#fname, (val##_obj)->handle, (name##_obj));
#define sem_nameable_value_len(fname, val, name, len) \
    sem_check_nameable_value_len_impl(#fname, (val##_obj)->handle, \
                                      (name##_obj), (len));
#define sem_va_arg(fname, list, ty) \
    sem_check_va_arg_impl(#fname, (list##_obj)->handle, (ty##_obj)->handle);
#define sem_cond_br(fname, builder, cond, then_bb, else_bb) \
    sem_check_cond_br_impl(#fname, (builder##_obj)->handle, \
                           (cond##_obj)->handle, (then_bb##_obj)->handle, \
                           (else_bb##_obj)->handle);
#define sem_builder_bb(fname, builder, bb) \
    do { \
        sem_expect_insert_position(#fname, (builder##_obj)->handle); \
        sem_expect_bb_of_builder(#fname, (builder##_obj)->handle, \
                                 (bb##_obj)->handle); \
    } while (0);
#define sem_invoke(fname, builder, ty, num_args, then_bb, catch_bb) \
    sem_check_invoke_impl(#fname, (builder##_obj)->handle, \
                          (ty##_obj)->handle, (num_args), \
                          (then_bb##_obj)->handle, (catch_bb##_obj)->handle);
#define sem_set_successor(fname, term, index, block) \
    do { \
        sem_check_successor_index_impl(#fname, (term##_obj)->handle, \
                                       (index)); \
        sem_expect_bb_of_inst(#fname, (term##_obj)->handle, \
                              (block##_obj)->handle); \
    } while (0);
#define sem_rmw_binop(fname, bin_op) \
    sem_expect_binop(#fname, (bin_op));
#define sem_inst_bb(fname, inst, bb) \
    sem_expect_bb_of_inst(#fname, (inst##_obj)->handle, (bb##_obj)->handle);
#define sem_bb_bb(fname, a, b) \
    FILC_CHECK(sem_parent_of_bb((a##_obj)->handle) == \
               sem_parent_of_bb((b##_obj)->handle), \
               NULL, \
               "Fil-C LLVM JIT API: wrong basic block for " #fname ": the " \
               "basic block belongs to a different function.");
#define sem_position(fname, block, inst) \
    FILC_CHECK(sem_parent_of_bb((block##_obj)->handle) == \
               sem_parent_of_inst((inst##_obj)->handle), \
               NULL, \
               "Fil-C LLVM JIT API: wrong basic block for " #fname ": the " \
               "basic block belongs to a different function.");
#define sem_cast(fname, op, val, dest_ty) \
    sem_check_cast_impl(#fname, (op), (val##_obj)->handle, \
                        (dest_ty##_obj)->handle);
#define sem_cast_op(fname, opcode, val, dest_ty) \
    sem_check_build_cast_impl(#fname, (opcode), (val##_obj)->handle, \
                              (dest_ty##_obj)->handle);
#define sem_int_cast(fname, val, dest_ty) \
    sem_check_int_cast_impl(#fname, (val##_obj)->handle, \
                            (dest_ty##_obj)->handle);
#define sem_pointer_cast(fname, val, dest_ty) \
    sem_check_pointer_cast_impl(#fname, (val##_obj)->handle, \
                                (dest_ty##_obj)->handle);
#define sem_const_pointer_cast(fname, val, dest_ty) \
    sem_check_const_pointer_cast_impl(#fname, (val##_obj)->handle, \
                                      (dest_ty##_obj)->handle);
#define sem_or_bit_cast(fname, op, val, dest_ty) \
    sem_check_or_bit_cast_impl(#fname, (op), (val##_obj)->handle, \
                               (dest_ty##_obj)->handle);
#define sem_extract_value(fname, agg, index) \
    sem_check_extract_value_impl(#fname, (agg##_obj)->handle, (index));
#define sem_aggregate_ret(fname, builder) \
    sem_check_aggregate_ret_impl(#fname, (builder##_obj)->handle);
#define sem_extract_element(fname, vec, index) \
    sem_check_extract_element_impl(#fname, (vec##_obj)->handle, \
                                   (index##_obj)->handle);
#define sem_insert_element(fname, vec, elt, index) \
    sem_check_insert_element_impl(#fname, (vec##_obj)->handle, \
                                  (elt##_obj)->handle, (index##_obj)->handle);
#define sem_shuffle_vector(fname, v1, v2, mask) \
    sem_check_shuffle_vector_impl(#fname, (v1##_obj)->handle, \
                                  (v2##_obj)->handle, (mask##_obj)->handle);
#define sem_const_unary_int(fname, val) \
    sem_check_const_unary_int_impl(#fname, (val##_obj)->handle);
#define sem_const_trunc(fname, val, dest_ty) \
    FILC_CHECK(sem_cast_valid(LLVMTrunc, sem_type_of((val##_obj)->handle), \
                               (LLVMTypeRef)(dest_ty##_obj)->handle), NULL, \
               "Fil-C LLVM JIT API: invalid cast for " #fname ": the cast " \
               "from the operand type to the destination type is not a valid " \
               "llvm::CastInst cast.");
#define sem_const_bit_cast(fname, val, dest_ty) \
    FILC_CHECK(sem_cast_valid(LLVMBitCast, sem_type_of((val##_obj)->handle), \
                               (LLVMTypeRef)(dest_ty##_obj)->handle), NULL, \
               "Fil-C LLVM JIT API: invalid cast for " #fname ": the cast " \
               "from the operand type to the destination type is not a valid " \
               "llvm::CastInst cast.");
#define sem_const_addr_space_cast(fname, val, dest_ty) \
    FILC_CHECK(sem_cast_valid(LLVMAddrSpaceCast, \
                               sem_type_of((val##_obj)->handle), \
                               (LLVMTypeRef)(dest_ty##_obj)->handle), NULL, \
               "Fil-C LLVM JIT API: invalid cast for " #fname ": the cast " \
               "from the operand type to the destination type is not a valid " \
               "llvm::CastInst cast.");
#define sem_const_trunc_or_bit_cast(fname, val, dest_ty) \
    FILC_CHECK(sem_cast_valid(LLVMBitCast, sem_type_of((val##_obj)->handle), \
                               (LLVMTypeRef)(dest_ty##_obj)->handle) || \
               sem_cast_valid(LLVMTrunc, sem_type_of((val##_obj)->handle), \
                               (LLVMTypeRef)(dest_ty##_obj)->handle), NULL, \
               "Fil-C LLVM JIT API: invalid cast for " #fname ": the cast " \
               "from the operand type to the destination type is not a valid " \
               "llvm::CastInst cast.");
#define sem_const_ptr_to_int(fname, val, dest_ty) \
    sem_check_const_ptr_to_int_impl(#fname, (val##_obj)->handle, \
                                    (dest_ty##_obj)->handle);
#define sem_const_int_to_ptr(fname, val, dest_ty) \
    sem_check_const_int_to_ptr_impl(#fname, (val##_obj)->handle, \
                                    (dest_ty##_obj)->handle);
#define sem_const_array(fname, elem_ty, vals, count) \
    sem_check_const_array_impl(#fname, (elem_ty##_obj)->handle, \
                               (void**)(vals##_obj), (unsigned long long)(count));
#define sem_const_named_struct(fname, struct_ty, vals, count) \
    sem_check_const_named_struct_impl(#fname, (struct_ty##_obj)->handle, \
                                      (void**)(vals##_obj), \
                                      (unsigned long long)(count));
#define sem_nonempty(fname, count) \
    sem_check_nonempty_impl(#fname, (count));
#define sem_fp_ty(fname, ty) \
    sem_check_fp_ty_impl(#fname, (ty##_obj)->handle);
#define sem_const_null(fname, ty) \
    sem_check_const_null_impl(#fname, (ty##_obj)->handle);
#define sem_const_all_ones(fname, ty) \
    sem_check_const_all_ones_impl(#fname, (ty##_obj)->handle);
#define sem_const_ptr_auth(fname, ptr, key, disc, addr_disc) \
    sem_check_const_ptr_auth_impl(#fname, (ptr##_obj)->handle, \
                                  (key##_obj)->handle, \
                                  (disc##_obj)->handle, \
                                  (addr_disc##_obj)->handle);
#define sem_const_extract_element(fname, vec, index) \
    sem_check_const_extract_element_impl(#fname, (vec##_obj)->handle, \
                                         (index##_obj)->handle);
#define sem_const_insert_element(fname, vec, elt, index) \
    sem_check_const_insert_element_impl(#fname, (vec##_obj)->handle, \
                                        (elt##_obj)->handle, \
                                        (index##_obj)->handle);
#define sem_const_shuffle_vector(fname, v1, v2, mask) \
    sem_check_const_shuffle_vector_impl(#fname, (v1##_obj)->handle, \
                                        (v2##_obj)->handle, \
                                        (mask##_obj)->handle);
#define sem_array_elem_ty(fname, elem_ty) \
    sem_check_array_elem_ty_impl(#fname, (elem_ty##_obj)->handle);
#define sem_vector_elem_ty(fname, elem_ty) \
    sem_check_vector_elem_ty_impl(#fname, (elem_ty##_obj)->handle);
/* FixedVectorType::get and ScalableVectorType::get assert NumElts > 0
 * ("#Elements of a VectorType must be greater than 0", Type.cpp).  LLVM puts
 * no upper bound on the count, so neither does this check. */
#define sem_vector_count(fname, count) \
    FILC_CHECK((count) >= 1, NULL, \
               "Fil-C LLVM JIT API: wrong element count for " #fname ": the " \
               "element count must be at least one.");
#define sem_fn_params(fname, params, count) \
    sem_check_fn_params_impl(#fname, (LLVMTypeRef const*)(params##_obj), \
                             (count));
#define sem_fn_ret_ty(fname, ret_ty) \
    sem_check_fn_ret_ty_impl(#fname, (ret_ty##_obj)->handle);
#define sem_global_ty(fname, ty) \
    sem_check_global_ty_impl(#fname, (ty##_obj)->handle);
#define sem_set_initializer(fname, global, val) \
    sem_check_set_initializer_impl(#fname, (global##_obj)->handle, \
                                   (val##_obj)->handle);
#define sem_operand_index(fname, user, index) \
    sem_check_operand_index_impl(#fname, (user##_obj)->handle, (index));
#define sem_incoming_index(fname, phi, index) \
    sem_check_incoming_index_impl(#fname, (phi##_obj)->handle, (index));
#define sem_successor_index(fname, term, index) \
    sem_check_successor_index_impl(#fname, (term##_obj)->handle, (index));
#define sem_mask_index(fname, shuffle, index) \
    sem_check_mask_index_impl(#fname, (shuffle##_obj)->handle, (index));
#define sem_arg_operand_index(fname, funclet, index) \
    sem_check_arg_operand_index_impl(#fname, (funclet##_obj)->handle, (index));
#define sem_param_index(fname, function, index) \
    sem_check_param_index_impl(#fname, (function##_obj)->handle, (index));
#define sem_callbr_indirect_dest_index(fname, callbr, index) \
    sem_check_callbr_indirect_dest_index_impl(#fname, \
                                              (callbr##_obj)->handle, \
                                              (index));
#define sem_md_operand_index(fname, mdnode, index) \
    sem_check_md_operand_index_impl(#fname, (mdnode##_obj)->handle, (index));
#define sem_target_ext_int_param_index(fname, ty, index) \
    sem_check_target_ext_param_index_impl( \
        #fname, (index), \
        api->LLVMGetTargetExtTypeNumIntParams((LLVMTypeRef)(ty##_obj)->handle));
#define sem_target_ext_type_param_index(fname, ty, index) \
    sem_check_target_ext_param_index_impl( \
        #fname, (index), \
        api->LLVMGetTargetExtTypeNumTypeParams( \
            (LLVMTypeRef)(ty##_obj)->handle));
#define sem_entries_index(fname, entries, index) \
    sem_check_entries_index_impl(#fname, (entries##_obj), (index));
#define sem_call_conv(fname, call_conv) \
    sem_check_call_conv_impl(#fname, (call_conv));
#define sem_struct_element_index(fname, struct_ty, index) \
    sem_check_struct_element_index_impl(#fname, (struct_ty##_obj)->handle, \
                                        (index));
#define sem_clause_index(fname, landing_pad, index) \
    sem_check_clause_index_impl(#fname, (landing_pad##_obj)->handle, (index));
#define sem_alignment(fname, value, bytes) \
    sem_check_alignment_impl(#fname, (value##_obj)->handle, (bytes));
#define sem_mem_align(fname, align) \
    sem_check_mem_align_impl(#fname, (align));
#define sem_nsw(fname, inst) \
    sem_check_nsw_impl(#fname, (inst##_obj)->handle);
#define sem_exact(fname, inst) \
    sem_check_exact_impl(#fname, (inst##_obj)->handle);
#define sem_nneg(fname, inst) \
    sem_check_nneg_impl(#fname, (inst##_obj)->handle);
#define sem_fpmath(fname, inst) \
    sem_check_fpmath_impl(#fname, (inst##_obj)->handle);
#define sem_rauw(fname, old_val, new_val) \
    sem_check_rauw_impl(#fname, (old_val##_obj)->handle, \
                        (new_val##_obj)->handle);
#define sem_module_flag_behavior(fname, behavior) \
    sem_check_module_flag_behavior_impl(#fname, (behavior));
#define sem_phi_ty(fname, ty) \
    sem_check_phi_ty_impl(#fname, (ty##_obj)->handle);
#define sem_unparented_bb(fname, bb) \
    sem_check_unparented_bb_impl(#fname, (bb##_obj)->handle);
#define sem_insert_bb(fname, builder, bb) \
    sem_check_insert_bb_impl(#fname, (builder##_obj)->handle, \
                             (bb##_obj)->handle);
#define sem_fp_cast(fname, val, dest_ty) \
    sem_check_fp_cast_impl(#fname, (val##_obj)->handle, \
                           (dest_ty##_obj)->handle);
#define sem_alias_aliasee(fname, alias, aliasee) \
    sem_check_alias_aliasee_impl(#fname, (alias##_obj)->handle, \
                                 (aliasee##_obj)->handle);
#define sem_alias_value_ty(fname, value_ty, addr_space, aliasee) \
    sem_check_alias_value_ty_impl(#fname, (aliasee##_obj)->handle, \
                                  (addr_space));
#define sem_enum_attr(fname, kind_id, val) \
    FILC_CHECK(api->ShimEnumAttributeArgsOK((kind_id), (val)) != 0, NULL, \
               "Fil-C LLVM JIT API: wrong attribute for " #fname ": the kind " \
               "must be an enum or int attribute kind, and the value must be " \
               "zero for enum attributes.");
#define sem_data_layout(fname, data_layout_str) \
    FILC_CHECK(api->ShimDataLayoutSpecOK((data_layout_str##_obj)) != 0, NULL, \
               "Fil-C LLVM JIT API: wrong data layout for " #fname ": the " \
               "data layout string is not a valid llvm::DataLayout.");
#define sem_apint_string(fname, int_ty, text, radix) \
    FILC_CHECK(api->ShimAPIntStringOK((LLVMTypeRef)(int_ty##_obj)->handle, \
                                      (text##_obj), strlen((text##_obj)), \
                                      (radix)) != 0, \
               NULL, \
               "Fil-C LLVM JIT API: wrong string for " #fname ": the string " \
               "is not a valid integer literal for the radix and the type " \
               "width.");
#define sem_apint_string_len(fname, int_ty, text, len, radix) \
    FILC_CHECK(api->ShimAPIntStringOK((LLVMTypeRef)(int_ty##_obj)->handle, \
                                      (text##_obj), (len), (radix)) != 0, \
               NULL, \
               "Fil-C LLVM JIT API: wrong string for " #fname ": the string " \
               "is not a valid integer literal for the radix and the type " \
               "width.");
#define sem_apfloat_string(fname, real_ty, text) \
    FILC_CHECK(api->ShimAPFloatStringOK((LLVMTypeRef)(real_ty##_obj)->handle, \
                                        (text##_obj), \
                                        strlen((text##_obj))) != 0, \
               NULL, \
               "Fil-C LLVM JIT API: wrong string for " #fname ": the string " \
               "is not a valid floating-point literal for the type.");
#define sem_apfloat_string_len(fname, real_ty, text, len) \
    FILC_CHECK(api->ShimAPFloatStringOK((LLVMTypeRef)(real_ty##_obj)->handle, \
                                        (text##_obj), (len)) != 0, \
               NULL, \
               "Fil-C LLVM JIT API: wrong string for " #fname ": the string " \
               "is not a valid floating-point literal for the type.");
#define sem_ptr_diff2(fname, lhs, rhs) \
    sem_check_ptr_diff2_impl(#fname, (lhs##_obj)->handle, (rhs##_obj)->handle);
#define sem_intrinsic_id(fname, id) \
    sem_check_intrinsic_id_impl(#fname, (id));
#define sem_intrinsic_tys_exact(fname, id, count) \
    sem_check_intrinsic_tys_exact_impl(#fname, (id), (count));
#define sem_intrinsic_tys_overloaded(fname, id, count) \
    sem_check_intrinsic_tys_overloaded_impl(#fname, (id), (count));
#define sem_intrinsic_tys_ok(fname, id, tys, count) \
    sem_check_intrinsic_tys_ok_impl(#fname, (id), \
                                    (LLVMTypeRef const*)(tys##_obj), (count));
#define sem_intrinsic_tys_mangle_ok(fname, tys, count) \
    sem_check_intrinsic_tys_mangle_ok_impl(#fname, \
                                           (LLVMTypeRef const*)(tys##_obj), \
                                           (count));
#define sem_atomic_inst(fname, inst) \
    sem_check_atomic_inst_impl(#fname, (inst##_obj)->handle);
#define sem_int_bits(fname, num_bits) \
    FILC_CHECK((num_bits) >= 1 && (num_bits) <= 8388608, NULL, \
               "Fil-C LLVM JIT API: wrong bit width for " #fname ": the bit " \
               "width must be between 1 and 8388608.");
#define sem_add_incoming(fname, phi, incoming_values, incoming_blocks, count) \
    sem_check_add_incoming_impl(#fname, (phi##_obj)->handle, \
                                (void**)(incoming_values##_obj), \
                                (void**)(incoming_blocks##_obj), (count));
#define sem_const_int_sext_fits(fname, constant_val) \
    FILC_CHECK(api->ShimConstIntSExtFits( \
                   (constant_val##_obj)->handle) != 0, NULL, \
               "Fil-C LLVM JIT API: wrong value for " #fname ": the " \
               "constant's signed value does not fit in an int64_t.");
#define sem_const_int_zext_fits(fname, constant_val) \
    FILC_CHECK(api->ShimConstIntZExtFits( \
                   (constant_val##_obj)->handle) != 0, NULL, \
               "Fil-C LLVM JIT API: wrong value for " #fname ": the " \
               "constant's value does not fit in a uint64_t.");
#define sem_aggregate_element(fname, value) \
    sem_check_aggregate_element_impl(#fname, (value##_obj)->handle);
#define sem_const_gep(fname, ty, indices, num_indices) \
    sem_check_const_gep_impl(#fname, (LLVMTypeRef)(ty##_obj)->handle, \
                             (void* const*)(indices##_obj), (num_indices));
#define sem_catchswitch_handlers(fname, num_handlers) \
    FILC_CHECK((num_handlers) >= 1, NULL, \
               "Fil-C LLVM JIT API: wrong handlers for " #fname ": the " \
               "handler count must be at least one.");
#define sem_inline_asm(fname, ty, constraints, constraints_size, dialect) \
    FILC_CHECK(api->ShimInlineAsmConstraintsOK((LLVMTypeRef)(ty##_obj)->handle, \
                                               (constraints##_obj), \
                                               (constraints_size)) != 0, \
               NULL, \
               "Fil-C LLVM JIT API: wrong constraints for " #fname ": the " \
               "constraint string is not a valid inline-asm constraint list " \
               "for the function type."); \
    /* InlineAsm keeps the dialect verbatim and LLVMGetInlineAsmDialect ends \
     * in an llvm_unreachable ("Unrecognized inline assembly dialect", \
     * llvm/lib/IR/Core.cpp) for anything but the two enum values. */ \
    FILC_CHECK((dialect) == LLVMInlineAsmDialectATT || \
               (dialect) == LLVMInlineAsmDialectIntel, NULL, \
               "Fil-C LLVM JIT API: wrong dialect for " #fname ": the " \
               "dialect must be LLVMInlineAsmDialectATT or " \
               "LLVMInlineAsmDialectIntel.");
#define sem_inline_asm_cstr(fname, ty, constraints) \
    FILC_CHECK(api->ShimInlineAsmConstraintsOK((LLVMTypeRef)(ty##_obj)->handle, \
                                               (constraints##_obj), \
                                               strlen((constraints##_obj))) != 0, \
               NULL, \
               "Fil-C LLVM JIT API: wrong constraints for " #fname ": the " \
               "constraint string is not a valid inline-asm constraint list " \
               "for the function type.");
/* LLVMStructSetBody needs two answers from the shim: whether the struct
 * accepts a body at all (StructType::setBody asserts isOpaque(), "Struct body
 * already set!", on an already-defined struct) and whether the body would
 * make the struct recursive (StructType::setBody cantFails on the checkBody
 * error).  The checks are separate so each reports its own message. */
#define sem_struct_set_body(fname, struct_ty, element_types, element_count) \
    FILC_CHECK(api->ShimStructIsOpaque( \
                   (LLVMTypeRef)(struct_ty##_obj)->handle) != 0, \
               NULL, \
               "Fil-C LLVM JIT API: wrong body for " #fname ": the struct " \
               "already has a body."); \
    FILC_CHECK(api->ShimStructSetBodyOK((LLVMTypeRef)(struct_ty##_obj)->handle, \
                                        (LLVMTypeRef*)(element_types##_obj), \
                                        (element_count)) != 0, \
               NULL, \
               "Fil-C LLVM JIT API: wrong body for " #fname ": the struct " \
               "body would make the struct recursive.");

/* CastInst::getCastOpcode's type-combination rule; see sem_cast_opcode_ok. */

#define sem_cast_opcode(fname, val, dest_ty) \
    sem_check_cast_opcode_impl(#fname, (val##_obj)->handle, \
                               (dest_ty##_obj)->handle);

/* PointerType::get's 24-bit address space field; shared by the pointer-type
 * creators and the global and ifunc creators. */

#define sem_address_space(fname, addr_space) \
    sem_check_address_space_impl(#fname, (addr_space));

#define sem_tail_call_kind(fname, kind) \
    sem_check_tail_call_kind_impl(#fname, (kind));

#define sem_param_alignment(fname, align) \
    sem_check_param_alignment_impl(#fname, (align));

#define sem_detached(fname, builder, inst) \
    sem_check_detached_impl(#fname, (builder##_obj)->handle, \
                            (inst##_obj)->handle);

#define sem_insert_position(fname, builder) \
    sem_expect_insert_position(#fname, (builder##_obj)->handle);

#define sem_type_attr_kind(fname, kind_id) \
    FILC_CHECK(api->ShimKindIsTypeAttr((kind_id)) != 0, NULL, \
               "Fil-C LLVM JIT API: wrong attribute kind for " #fname ": the " \
               "kind must be a type attribute kind.");

#define sem_nameable_ty(fname, ty, name) \
    sem_check_nameable_ty_impl(#fname, (ty##_obj)->handle, (name##_obj));

#define sem_catchswitch_unwind(fname, builder, unwind_bb) \
    sem_check_catchswitch_unwind_impl(#fname, (builder##_obj)->handle, \
                                      (unwind_bb##_obj)->handle);

/* Null-element checks.  An ARR descriptor's trailing token selects both one
 * of these and one of the element checks above; every ARR token must define
 * both, and a missing one is a compile error.  NONE rejects null elements
 * (the default: those LLVM implementations dereference their elements
 * unconditionally), while ElemNullOK and ElemMDNodeOperand permit them,
 * because the MDNode constructors map null elements to null metadata. */

#define FILC_LLVM_JIT_VAL_NULL_ELEM_NONE(fname, name, count_name) \
    do { \
        size_t filc_llvm_jit_null_index; \
        for (filc_llvm_jit_null_index = 0; \
             filc_llvm_jit_null_index < (size_t)(count_name); \
             ++filc_llvm_jit_null_index) { \
            FILC_CHECK(name##_obj[filc_llvm_jit_null_index], NULL, \
                       "Fil-C LLVM JIT API: null element in " #name \
                       " argument of " #fname "."); \
        } \
    } while (0);

#define FILC_LLVM_JIT_VAL_NULL_ELEM_Constant(fname, name, count_name) \
    FILC_LLVM_JIT_VAL_NULL_ELEM_NONE(fname, name, count_name)

#define FILC_LLVM_JIT_VAL_NULL_ELEM_ElemNullOK(fname, name, count_name)

#define FILC_LLVM_JIT_VAL_NULL_ELEM_ElemMDNodeOperand(fname, name, count_name)

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
    FILC_LLVM_JIT_VAL_ConstantFP(LLVMConstRealGetDouble,
                                 (LLVMValueRef)value->handle);
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
    FILC_LLVM_JIT_VAL_InstructionOrGlobalObject(
        LLVMGlobalCopyAllMetadata, (LLVMValueRef)value->handle);
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
                                                 filc_ptr entries_ptr, unsigned index,
                                                 filc_ptr len_ptr)
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
    size_t key_len = 0;
    filc_check_write(len_ptr, sizeof(size_t));
    pas_lock_lock(&context->lock);
    /* The entries array is indexed raw, and the object records its length
       (see LLVMCopyModuleFlagsMetadata), so an out-of-range index reads past
       LLVM-owned memory. */
    sem_check_entries_index_impl("LLVMModuleFlagEntriesGetKey", entries, index);
    /* LLVM's LLVMModuleFlagEntriesGetKey writes *Len unconditionally, so a
       real local must be passed; NULL would segfault inside LLVM. */
    key = api->LLVMModuleFlagEntriesGetKey((LLVMModuleFlagEntry*)entries->handle, index,
                                           &key_len);
    filc_ptr result = key ? filc_strdup(my_thread, key) : filc_ptr_forge_null();
    pas_lock_unlock(&context->lock);
    /* The out parameter is written even though the returned Fil-C string
       knows its own length. */
    *(size_t*)filc_ptr_ptr(len_ptr) = key_len;
    return result;
}

filc_ptr filc_native_LLVMCopyModuleFlagsMetadata(filc_thread* my_thread,
                                                 filc_ptr module_ptr,
                                                 filc_ptr len_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = unwrap_obj(
        my_thread, module_ptr, FILC_LLVM_JIT_KIND_MODULE, &context, NULL);
    size_t len = 0;
    LLVMModuleFlagEntry* entries = NULL;
    filc_check_write(len_ptr, sizeof(size_t));
    pas_lock_lock(&context->lock);
    entries = api->LLVMCopyModuleFlagsMetadata((LLVMModuleRef)module->handle, &len);
    /* The entries array is LLVM-owned until LLVMDisposeModuleFlagsMetadata;
       the wrapper records the count so the getters can bounds-check. */
    filc_ptr result = wrap_handle(my_thread, context, module,
                                  FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES,
                                  (void*)entries);
    if (filc_ptr_ptr(result)) {
        ((filc_llvm_jit_entries_object*)filc_ptr_ptr(result))->count = len;
    }
    pas_lock_unlock(&context->lock);
    *(size_t*)filc_ptr_ptr(len_ptr) = len;
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
                   "Fil-C LLVM JIT API: LLVMConstIntOfArbitraryPrecision: null "
                   "words with nonzero count.");
    }
    LLVMValueRef result_raw;
    pas_lock_lock(&context->lock);
    FILC_LLVM_JIT_VAL_IntegerTypeKind(LLVMConstIntOfArbitraryPrecision,
                                      (LLVMTypeRef)int_ty->handle);
    /* APInt::initFromArray asserts "Null pointer detected!" when the word
       array is null, and both it and the APInt(ArrayRef<uint64_t>)
       constructor index the words unconditionally, so an empty word array
       dies in ArrayRef::operator[] ("Invalid index!", ArrayRef.h) even with
       a non-null pointer.  LLVM derives no meaning from a zero-word APInt
       here, so the word count must be at least one and the pointer must not
       be null. */
    FILC_CHECK(num_words != 0 && words != NULL, NULL,
               "Fil-C LLVM JIT API: LLVMConstIntOfArbitraryPrecision: the "
               "word count must be at least one and the words pointer must "
               "not be null.");
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
                   "Fil-C LLVM JIT API: LLVMCreateConstantRangeAttribute: null "
                   "lower words with nonzero count.");
    }
    if (filc_ptr_ptr(upper_words_ptr)) {
        filc_check_read(upper_words_ptr, filc_mul_size(num_words, sizeof(uint64_t)));
        upper_words = (const uint64_t*)filc_ptr_ptr(upper_words_ptr);
    } else {
        FILC_CHECK(!num_words, NULL,
                   "Fil-C LLVM JIT API: LLVMCreateConstantRangeAttribute: null "
                   "upper words with nonzero count.");
    }
    LLVMAttributeRef result_raw;
    pas_lock_lock(&context->lock);
    /* Three checks, mirroring the asserts that LLVM would otherwise hit:
       Attribute::get asserts isConstantRangeAttrKind (Attributes.cpp), APInt
       asserts "Null pointer detected!" for a zero bit width, and
       ConstantRange asserts that an empty (lower == upper) range is either
       empty (all zero) or full (all ones).  The kind check is a shim
       predicate, because the attribute-kind enum is LLVM-internal. */
    FILC_CHECK(api->ShimKindIsConstantRangeAttr(kind_id) != 0, NULL,
               "Fil-C LLVM JIT API: LLVMCreateConstantRangeAttribute: the kind "
               "must be a constant-range attribute kind.");
    FILC_CHECK(num_bits != 0, NULL,
               "Fil-C LLVM JIT API: LLVMCreateConstantRangeAttribute: the bit "
               "width must not be zero.");
    {
        unsigned word_index;
        bool equal;
        bool all_zero;
        bool all_ones;
        equal = true;
        all_zero = true;
        all_ones = true;
        for (word_index = 0; word_index < num_words; ++word_index) {
            if (lower_words[word_index] != upper_words[word_index])
                equal = false;
            if (lower_words[word_index] != 0)
                all_zero = false;
            if (lower_words[word_index] != UINT64_MAX)
                all_ones = false;
        }
        FILC_CHECK(!equal || all_zero || all_ones, NULL,
                   "Fil-C LLVM JIT API: LLVMCreateConstantRangeAttribute: the "
                   "lower and upper bounds must differ unless the range is "
                   "empty or full.");
    }
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
    FILC_LLVM_JIT_VAL_FunctionTypeKind(LLVMBuildCallBr, (LLVMTypeRef)ty->handle)
    /* CallBrInst::init asserts the call-signature rule (Instructions.cpp), and
       the destination blocks must belong to the function that the builder is
       building into (a block of another function is only observable when
       CompileModule clones the module, and the clone asserts first). */
    sem_check_call_sig_impl("LLVMBuildCallBr", (LLVMTypeRef)handle_of(ty),
                            num_args);
    /* The built callbr names its result value; a void return type would abort
       in Value.cpp before anything else can run. */
    sem_check_nameable_result_impl("LLVMBuildCallBr", handle_of(ty), name);
    sem_expect_insert_position("LLVMBuildCallBr", (LLVMBuilderRef)builder->handle);
    sem_expect_bb_of_builder("LLVMBuildCallBr", (LLVMBuilderRef)builder->handle,
                             (LLVMBasicBlockRef)handle_of(default_dest));
    {
        unsigned dest_index;
        for (dest_index = 0; dest_index < num_indirect_dests; ++dest_index) {
            sem_expect_bb_of_builder(
                "LLVMBuildCallBr", (LLVMBuilderRef)builder->handle,
                (LLVMBasicBlockRef)indirect_dests[dest_index]);
        }
    }
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
   companion count and the getter into a host scratch buffer (under the context
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
    LLVMAttributeRef* raws = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    FILC_LLVM_JIT_VAL_Function(LLVMGetAttributesAtIndex, (LLVMValueRef)fn->handle)
    count = api->LLVMGetAttributeCountAtIndex((LLVMValueRef)fn->handle, index);
    if (count)
        raws = (LLVMAttributeRef*)filc_bmalloc_allocate_tmp(my_thread, (size_t)count * sizeof(LLVMAttributeRef));
    if (!count || raws)
        api->LLVMGetAttributesAtIndex((LLVMValueRef)fn->handle, index, raws);
    if (count && !raws) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetAttributesAtIndex.");
    }
    emit_out_array(my_thread, context, module, (void**)raws, count, attrs_ptr,
                   FILC_LLVM_JIT_KIND_ATTRIBUTE);
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
    LLVMAttributeRef* raws = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    FILC_LLVM_JIT_VAL_CallBase(LLVMGetCallSiteAttributes, (LLVMValueRef)call->handle)
    count = api->LLVMGetCallSiteAttributeCount((LLVMValueRef)call->handle, index);
    if (count)
        raws = (LLVMAttributeRef*)filc_bmalloc_allocate_tmp(my_thread, (size_t)count * sizeof(LLVMAttributeRef));
    if (!count || raws)
        api->LLVMGetCallSiteAttributes((LLVMValueRef)call->handle, index, raws);
    if (count && !raws) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetCallSiteAttributes.");
    }
    emit_out_array(my_thread, context, module, (void**)raws, count, attrs_ptr,
                   FILC_LLVM_JIT_KIND_ATTRIBUTE);
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
    LLVMBasicBlockRef* raws = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    FILC_LLVM_JIT_VAL_Function(LLVMGetBasicBlocks, (LLVMValueRef)fn->handle)
    count = api->LLVMCountBasicBlocks((LLVMValueRef)fn->handle);
    if (count)
        raws = (LLVMBasicBlockRef*)filc_bmalloc_allocate_tmp(my_thread, (size_t)count * sizeof(LLVMBasicBlockRef));
    if (!count || raws)
        api->LLVMGetBasicBlocks((LLVMValueRef)fn->handle, raws);
    if (count && !raws) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetBasicBlocks.");
    }
    emit_out_array(my_thread, context, module, (void**)raws, count, blocks_ptr,
                   FILC_LLVM_JIT_KIND_BASIC_BLOCK);
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
    LLVMBasicBlockRef* raws = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    FILC_LLVM_JIT_VAL_CatchSwitchInst(LLVMGetHandlers,
                                      (LLVMValueRef)catch_switch->handle);
    count = api->LLVMGetNumHandlers((LLVMValueRef)catch_switch->handle);
    if (count)
        raws = (LLVMBasicBlockRef*)filc_bmalloc_allocate_tmp(my_thread, (size_t)count * sizeof(LLVMBasicBlockRef));
    if (!count || raws)
        api->LLVMGetHandlers((LLVMValueRef)catch_switch->handle, raws);
    if (count && !raws) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetHandlers.");
    }
    emit_out_array(my_thread, context, module, (void**)raws, count, handlers_ptr,
                   FILC_LLVM_JIT_KIND_BASIC_BLOCK);
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
    LLVMValueRef* raws = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    FILC_LLVM_JIT_VAL_MDNodeOrValueAsMetadataValue(
        LLVMGetMDNodeOperands, (LLVMValueRef)value->handle);
    count = api->LLVMGetMDNodeNumOperands((LLVMValueRef)value->handle);
    if (count)
        raws = (LLVMValueRef*)filc_bmalloc_allocate_tmp(my_thread, (size_t)count * sizeof(LLVMValueRef));
    if (!count || raws)
        api->LLVMGetMDNodeOperands((LLVMValueRef)value->handle, raws);
    if (count && !raws) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetMDNodeOperands.");
    }
    emit_out_array(my_thread, context, module, (void**)raws, count, dest_ptr,
                   FILC_LLVM_JIT_KIND_VALUE);
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
    LLVMValueRef* raws = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    count = api->LLVMGetNamedMetadataNumOperands((LLVMModuleRef)module_obj->handle, name);
    if (count)
        raws = (LLVMValueRef*)filc_bmalloc_allocate_tmp(my_thread, (size_t)count * sizeof(LLVMValueRef));
    if (!count || raws)
        api->LLVMGetNamedMetadataOperands((LLVMModuleRef)module_obj->handle, name, raws);
    if (count && !raws) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetNamedMetadataOperands.");
    }
    emit_out_array(my_thread, context, module_obj, (void**)raws, count,
                                 dest_ptr, FILC_LLVM_JIT_KIND_VALUE);
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
    LLVMTypeRef* raws = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    FILC_LLVM_JIT_VAL_FunctionTypeKind(LLVMGetParamTypes,
                                       (LLVMTypeRef)function_ty->handle);
    count = api->LLVMCountParamTypes((LLVMTypeRef)function_ty->handle);
    if (count)
        raws = (LLVMTypeRef*)filc_bmalloc_allocate_tmp(my_thread, (size_t)count * sizeof(LLVMTypeRef));
    if (!count || raws)
        api->LLVMGetParamTypes((LLVMTypeRef)function_ty->handle, raws);
    if (count && !raws) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetParamTypes.");
    }
    emit_out_array(my_thread, context, module, (void**)raws, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_TYPE);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMGetParams(filc_thread* my_thread, filc_ptr fn_ptr, filc_ptr dest_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* fn = unwrap_obj(
        my_thread, fn_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* raws = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    FILC_LLVM_JIT_VAL_Function(LLVMGetParams, (LLVMValueRef)fn->handle)
    count = api->LLVMCountParams((LLVMValueRef)fn->handle);
    if (count)
        raws = (LLVMValueRef*)filc_bmalloc_allocate_tmp(my_thread, (size_t)count * sizeof(LLVMValueRef));
    if (!count || raws)
        api->LLVMGetParams((LLVMValueRef)fn->handle, raws);
    if (count && !raws) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetParams.");
    }
    emit_out_array(my_thread, context, module, (void**)raws, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_VALUE);
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
    LLVMTypeRef* raws = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    FILC_LLVM_JIT_VAL_StructTypeKind(LLVMGetStructElementTypes,
                                     (LLVMTypeRef)struct_ty->handle);
    count = api->LLVMCountStructElementTypes((LLVMTypeRef)struct_ty->handle);
    if (count)
        raws = (LLVMTypeRef*)filc_bmalloc_allocate_tmp(my_thread, (size_t)count * sizeof(LLVMTypeRef));
    if (!count || raws)
        api->LLVMGetStructElementTypes((LLVMTypeRef)struct_ty->handle, raws);
    if (count && !raws) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetStructElementTypes.");
    }
    emit_out_array(my_thread, context, module, (void**)raws, count, dest_ptr,
                   FILC_LLVM_JIT_KIND_TYPE);
    pas_lock_unlock(&context->lock);
}

void filc_native_LLVMGetSubtypes(filc_thread* my_thread, filc_ptr ty_ptr, filc_ptr dest_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* ty = unwrap_obj(
        my_thread, ty_ptr, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef* raws = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->lock);
    count = api->LLVMGetNumContainedTypes((LLVMTypeRef)ty->handle);
    if (count)
        raws = (LLVMTypeRef*)filc_bmalloc_allocate_tmp(my_thread, (size_t)count * sizeof(LLVMTypeRef));
    if (!count || raws)
        api->LLVMGetSubtypes((LLVMTypeRef)ty->handle, raws);
    if (count && !raws) {
        pas_lock_unlock(&context->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetSubtypes.");
    }
    emit_out_array(my_thread, context, module, (void**)raws, count, dest_ptr,
                   FILC_LLVM_JIT_KIND_TYPE);
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
/* Strings and arrays that the C API returns as a plain pointer.  Whatever
   the LLVM side hands back, the wrapper copies the bytes into fresh Fil-C
   memory while entered; the length out-params of the C API are dropped
   because Fil-C values carry their own bounds.  The copies fall into two
   classes:

   - Strings owned by the LLVM objects themselves (names, sections, data
     layout strings, inline asm text, ...): the pointer stays valid for the
     parent object's lifetime, so there is nothing to free after copying.
     These use FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0/LEN.

   - Strings LLVM freshly allocated on the caller's behalf
     (LLVMPrintModuleToString, LLVMPrintValueToString,
     LLVMIntrinsicCopyOverloadedName2): the C API contract is that the
     caller releases them with LLVMDisposeMessage.  LLVMDisposeMessage is
     in the generated api table (filc/llvmjit/filc_llvm_jit_api.h -- the
     generated headers carry the full llvm-c/Core.h surface, disposes
     included; only the hand-curated USER header filc/include/filc_llvm_jit.h
     is selective).  These use FILC_LLVM_JIT_DEFINE_OWNED_CSTR0 or dispose
     inline: the LLVM-side original is released right after the copy, so
     no LLVM-allocated memory leaks. */

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

#define FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(c_name, kind, ...) \
    filc_ptr filc_native_##c_name(filc_thread* my_thread, filc_ptr parent_ptr) \
    { \
        ensure_api(my_thread); \
        filc_llvm_jit_context_object* context = NULL; \
        filc_llvm_jit_object* module = NULL; \
        filc_llvm_jit_object* parent = unwrap_obj( \
            my_thread, parent_ptr, kind, &context, &module); \
        const char* raw; \
        pas_lock_lock(&context->lock); \
        FILC_LLVM_JIT_VALIDATE_BORROWED(c_name, parent, ##__VA_ARGS__) \
        raw = api->c_name((void*)parent->handle); \
        filc_ptr result = filc_strdup(my_thread, raw); \
        pas_lock_unlock(&context->lock); \
        return result; \
    }

#define FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(c_name, kind, len_type, ...) \
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
        FILC_LLVM_JIT_VALIDATE_BORROWED(c_name, parent, ##__VA_ARGS__) \
        raw = api->c_name((void*)parent->handle, &length); \
        filc_ptr result = \
            new_sized_string(my_thread, raw, (size_t)length); \
        pas_lock_unlock(&context->lock); \
        return result; \
    }

/* Like BORROWED_CSTR0, but for the C APIs that return a freshly
 * LLVM-allocated string (LLVMCreateMessage under the hood): after copying
 * into Fil-C memory, the LLVM-side original is released with
 * LLVMDisposeMessage (available in the generated api table), so nothing
 * LLVM-allocated leaks. */
#define FILC_LLVM_JIT_DEFINE_OWNED_CSTR0(c_name, kind, ...) \
    filc_ptr filc_native_##c_name(filc_thread* my_thread, filc_ptr parent_ptr) \
    { \
        ensure_api(my_thread); \
        filc_llvm_jit_context_object* context = NULL; \
        filc_llvm_jit_object* module = NULL; \
        filc_llvm_jit_object* parent = unwrap_obj( \
            my_thread, parent_ptr, kind, &context, &module); \
        char* raw; \
        pas_lock_lock(&context->lock); \
        FILC_LLVM_JIT_VALIDATE_BORROWED(c_name, parent, ##__VA_ARGS__) \
        raw = api->c_name((void*)parent->handle); \
        filc_ptr result = filc_strdup(my_thread, raw); \
        if (raw) \
            api->LLVMDisposeMessage(raw); \
        pas_lock_unlock(&context->lock); \
        return result; \
    }

/* The borrowed-string wrappers take the same check token as a row
 * descriptor, applied to the parent object's handle; pass no token when the
 * LLVM implementation is graceful for the filc kind. */
#define FILC_LLVM_JIT_VALIDATE_BORROWED(fname, parent, ...) \
    FILC_LLVM_JIT_VALIDATE_BORROWED_2(fname, (void*)(parent)->handle, \
                                      ##__VA_ARGS__, NONE)
#define FILC_LLVM_JIT_VALIDATE_BORROWED_2(fname, handle, marker, ...) \
    FILC_LLVM_JIT_VAL_##marker(fname, handle)

FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetBasicBlockName, FILC_LLVM_JIT_KIND_BASIC_BLOCK)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetDataLayout, FILC_LLVM_JIT_KIND_MODULE)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetDataLayoutStr, FILC_LLVM_JIT_KIND_MODULE)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetGC, FILC_LLVM_JIT_KIND_VALUE, Function)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetSection, FILC_LLVM_JIT_KIND_VALUE, GlobalValue)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetStructName, FILC_LLVM_JIT_KIND_TYPE, StructTypeKind)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetTarget, FILC_LLVM_JIT_KIND_MODULE)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetTargetExtTypeName, FILC_LLVM_JIT_KIND_TYPE,
                                    TargetExtTypeKind)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR0(LLVMGetValueName, FILC_LLVM_JIT_KIND_VALUE)
/* Fil-C: added for mesa/llvmpipe's debug dumps.  These C APIs return a
 * malloc'd string that the caller would free with LLVMDisposeMessage; the
 * wrappers copy the bytes into fresh Fil-C memory (filc_strdup) and then
 * dispose the LLVM-side original -- the LLVM-allocated string does not
 * leak. */
FILC_LLVM_JIT_DEFINE_OWNED_CSTR0(LLVMPrintModuleToString, FILC_LLVM_JIT_KIND_MODULE)
FILC_LLVM_JIT_DEFINE_OWNED_CSTR0(LLVMPrintValueToString, FILC_LLVM_JIT_KIND_VALUE)

FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetDebugLocDirectory, FILC_LLVM_JIT_KIND_VALUE,
                                       unsigned, DebugLocOwner)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetDebugLocFilename, FILC_LLVM_JIT_KIND_VALUE,
                                       unsigned, DebugLocOwner)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetInlineAsmAsmString, FILC_LLVM_JIT_KIND_VALUE,
                                       size_t, InlineAsm)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetInlineAsmConstraintString,
                                       FILC_LLVM_JIT_KIND_VALUE, size_t, InlineAsm)
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
                                       FILC_LLVM_JIT_KIND_ATTRIBUTE, unsigned,
                                       StringAttribute)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetStringAttributeValue,
                                       FILC_LLVM_JIT_KIND_ATTRIBUTE, unsigned,
                                       StringAttribute)
FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(LLVMGetValueName2, FILC_LLVM_JIT_KIND_VALUE, size_t)

filc_ptr filc_native_LLVMIntrinsicGetName(filc_thread* my_thread, unsigned id,
                                          filc_ptr name_length_ptr)
{
    ensure_api(my_thread);
    size_t length = 0;
    const char* raw;
    filc_check_write(name_length_ptr, sizeof(size_t));
    /* llvm_map_to_intrinsic_id asserts "Intrinsic ID out of range"
       (Core.cpp) for an id that is not a known intrinsic. */
    sem_check_intrinsic_id_impl("LLVMIntrinsicGetName", id);
    /* Intrinsic::getName(ID) asserts "This version of getName does not
       support overloading" (Intrinsics.cpp) for an overloaded id. */
    FILC_CHECK(api->LLVMIntrinsicIsOverloaded(id) == 0, NULL,
               "Fil-C LLVM JIT API: wrong intrinsic id for "
               "LLVMIntrinsicGetName: the id must be a non-overloaded "
               "intrinsic.");
    raw = api->LLVMIntrinsicGetName(id, &length);
    filc_ptr result = new_sized_string(my_thread, raw, length);
    *(size_t*)filc_ptr_ptr(name_length_ptr) = length;
    return result;
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
    FILC_LLVM_JIT_VAL_ExtractValueOrInsertValue(LLVMGetIndices,
                                                (LLVMValueRef)inst->handle);
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
    FILC_LLVM_JIT_VAL_Instruction(
        LLVMInstructionGetAllMetadataOtherThanDebugLoc,
        (LLVMValueRef)inst->handle);
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

filc_ptr filc_native_LLVMIntrinsicCopyOverloadedName2(
    filc_thread* my_thread, filc_ptr module_ptr, unsigned id,
    filc_ptr param_types_ptr, size_t param_count, filc_ptr name_length_ptr)
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
    filc_check_write(name_length_ptr, sizeof(size_t));
    pas_lock_lock(&context->lock);
    /* llvm_map_to_intrinsic_id asserts "Intrinsic ID out of range"
       (Core.cpp) for an id that is not a known intrinsic. */
    sem_check_intrinsic_id_impl("LLVMIntrinsicCopyOverloadedName2", id);
    /* getIntrinsicNameImpl asserts "This version of getName is for
       overloaded intrinsics only" (Intrinsics.cpp) for a non-overloaded id
       with Tys; the exact rule keeps the Copy2 native in lockstep with
       LLVMGetIntrinsicDeclaration. */
    sem_check_intrinsic_tys_exact_impl("LLVMIntrinsicCopyOverloadedName2", id,
                                       param_count);
    /* The Tys also feed the type-table decode (through the unnamed-type
       branch of getIntrinsicNameImpl) and the name mangling
       (getMangledTypeStr llvm_unreachables for label and token types); the
       same rules LLVMGetIntrinsicDeclaration enforces apply here. */
    sem_check_intrinsic_tys_ok_impl("LLVMIntrinsicCopyOverloadedName2", id,
                                    (LLVMTypeRef const*)param_types,
                                    param_count);
    sem_check_intrinsic_tys_mangle_ok_impl("LLVMIntrinsicCopyOverloadedName2",
                                           (LLVMTypeRef const*)param_types,
                                           param_count);
    raw = api->LLVMIntrinsicCopyOverloadedName2((LLVMModuleRef)module_obj->handle, id,
                                                param_types, param_count, &name_length);
    filc_ptr result = new_sized_string(my_thread, raw, name_length);
    /* LLVMIntrinsicCopyOverloadedName2 hands ownership of a freshly
       allocated string to the caller (release it with LLVMDisposeMessage).
       We copied it into Fil-C memory, so release the LLVM-side original:
       LLVMDisposeMessage is in the generated api table. */
    if (raw)
        api->LLVMDisposeMessage(raw);
    pas_lock_unlock(&context->lock);
    *(size_t*)filc_ptr_ptr(name_length_ptr) = name_length;
    return result;
}

/* ------------------------------------------------------------------ */
/* Thin wrappers for the curated LLVM C API, defined by the row table below.
 *
 * Every wrapper in this section is defined by exactly one row of the table.
 * A row names the LLVM C API entry point (the wrapper itself is always
 * filc_native_<entry point>, per filc_native.h), selects the locking
 * discipline, and describes every parameter with a descriptor:
 *
 *   (CTX, name)
 *       A filc_ptr parameter holding a JIT context object, unwrapped with
 *       unwrap_context(); it establishes the LLVM context that all other
 *       objects in the call must agree on.
 *   (REF, name, KIND, LLVMXxxRef)
 *       A filc_ptr parameter holding a JIT object of kind
 *       FILC_LLVM_JIT_KIND_KIND, unwrapped with unwrap_obj().  It is passed
 *       to the LLVM C API as (LLVMXxxRef)handle_of(name_obj).
 *   (REF, name, KIND, LLVMXxxRef, Check)
 *       Same as above, plus a check token: the wrapper FILC_CHECKs that the
 *       object really is the C++ subclass that the LLVM implementation
 *       demands, panicking with a "wrong LLVM ... subclass" message instead
 *       of tripping an LLVM cast<> assertion.  See the FILC_LLVM_JIT_VAL_*
 *       macros above for the token vocabulary.  Omit the token when the LLVM
 *       implementation accepts any object of the filc kind (base-typed
 *       parameters, graceful dyn_cast/isa implementations, or kind-pinned
 *       handles).
 *   (CSTR, name)
 *       A filc_ptr parameter converted to a borrowed NUL-terminated string
 *       with filc_check_and_get_tmp_str().
 *   (OPTSTR, name, len_name)
 *       A borrowed string that may be null, as long as the separately
 *       passed length len_name is zero; len_name must name a (SCAL, ...)
 *       descriptor elsewhere in the row.
 *   (ARR, name, count_name, KIND, LLVMXxxRef)
 *       A filc_ptr parameter holding an array of JIT objects of kind
 *       FILC_LLVM_JIT_KIND_KIND, decoded into a temporary array of raw
 *       handles with decode_handle_array(); count_name must name a (SCAL,
 *       ...) descriptor elsewhere in the row.
 *   (ARR, name, count_name, KIND, LLVMXxxRef, Check)
 *       Same as above, plus a check token: every element is FILC_CHECKed to
 *       be non-null and to be the required C++ subclass.  An empty array
 *       (including a null array with a zero count) checks nothing.  The
 *       special tokens ElemNullOK and ElemMDNodeOperand opt out of the
 *       null-element check, because the LLVM implementations that get them
 *       (the MDNode constructors) accept null elements; ElemNullOK applies
 *       no subclass check, while ElemMDNodeOperand applies the
 *       LLVMMDNodeInContext element rules; see the
 *       FILC_LLVM_JIT_VAL_NULL_ELEM_* and FILC_LLVM_JIT_VAL_ELEM_* macros.
 *   (SCAL, name, type)
 *       A plain C parameter of the given type, passed through unchanged.
 *   (ADOPT, name)
 *       Trailing row marker: after the api-> call, have the builder named
 *       by a (REF, name, BUILDER, LLVMBuilderRef) descriptor adopt the
 *       module it is building into, if any.
 *
 * The defining macro is FILC_LLVM_JIT_DEFINE_<shape>_<arity>, where shape
 * is VOID, REF (returns a fresh filc_ptr via wrap_handle(); the row carries
 * the wrap kind), or SCALAR (returns a plain C value; the row carries the
 * return type).  The second row argument selects the locking discipline:
 *
 *   LOCKED           Lock the LLVM context established by the unwraps.
 *   LOCK_IF_CONTEXT  Lock the context only if one was established.  Used
 *                    when a null or empty array input means there is no
 *                    context at all.
 *   NO_LOCK          No context can exist; run lock-free.
 *
 * After the unwraps and the lock, each row's descriptors are passed to
 * FILC_LLVM_JIT_VALIDATE, which expands to nothing for descriptors without
 * a check token, to one FILC_CHECK for a REF descriptor with a token, and to
 * a per-element loop (which also rejects null elements, except for rows
 * carrying the ElemNullOK or ElemMDNodeOperand token) for an ARR descriptor
 * with a token.  Validation therefore happens inside the locked
 * region, immediately before the api-> call, where the subclass reads are
 * as race-free as the call itself.
 *
 * A row may additionally carry one trailing semantic-check clause (a bare
 * helper invocation; see the semantic-check section above).  It expands
 * right after the VALIDATEs and before the api-> call, inside the same
 * locked region, so a cross-argument rule violation panics before LLVM can
 * assert.  The DEFINE macros are variadic for this purpose; rows without a
 * clause are unaffected.
 *
 * Descriptor locals are named name##_obj; the context and module that the
 * unwraps thread through are llvm_context and llvm_module.  All helper
 * macros here expand to whole statements, so invocations are written
 * without trailing semicolons.
 *
 * This table is the source of truth for these wrappers and is meant to be
 * edited by hand; no generator regenerates this section.  When adding a
 * wrapper for a new LLVM C API entry point, add a row here, a declaration
 * to filc/include/filc_llvm_jit.h, and an addSig entry to
 * libpas/src/libpas/generate_pizlonated_forwarders.rb.
 */

/* Descriptor expansion: turn (KIND, args...) into a parameter declaration,
 * an unwrap statement, or a call argument, depending on the mode. */
#define FILC_LLVM_JIT_PARAM(d) FILC_LLVM_JIT_PARAM_I d
#define FILC_LLVM_JIT_PARAM_I(kind, ...) FILC_LLVM_JIT_PARAM_##kind(__VA_ARGS__)
#define FILC_LLVM_JIT_PARAM_CTX(name) filc_ptr name
#define FILC_LLVM_JIT_PARAM_REF(name, kind, ref_ty, ...) filc_ptr name
#define FILC_LLVM_JIT_PARAM_CSTR(name) filc_ptr name
#define FILC_LLVM_JIT_PARAM_OPTSTR(name, len_name) filc_ptr name
#define FILC_LLVM_JIT_PARAM_ARR(name, count_name, kind, elem_ty, ...) filc_ptr name
#define FILC_LLVM_JIT_PARAM_SCAL(name, type) type name

#define FILC_LLVM_JIT_ARG(d) FILC_LLVM_JIT_ARG_I d
#define FILC_LLVM_JIT_ARG_I(kind, ...) FILC_LLVM_JIT_ARG_##kind(__VA_ARGS__)
#define FILC_LLVM_JIT_ARG_CTX(name) (LLVMContextRef)name##_obj->handle
#define FILC_LLVM_JIT_ARG_REF(name, kind, ref_ty, ...) (ref_ty)handle_of(name##_obj)
#define FILC_LLVM_JIT_ARG_CSTR(name) name##_obj
#define FILC_LLVM_JIT_ARG_OPTSTR(name, len_name) name##_obj
#define FILC_LLVM_JIT_ARG_ARR(name, count_name, kind, elem_ty, ...) name##_obj
#define FILC_LLVM_JIT_ARG_SCAL(name, type) name

/* The function name rides along so that OPTSTR can stringify it into its
 * FILC_CHECK message.  STRIP flattens the descriptor tuple so that the kind
 * can be pasted onto the per-kind helper macro name. */
#define FILC_LLVM_JIT_UNWRAP(fname, d) \
    FILC_LLVM_JIT_UNWRAP_2(fname, FILC_LLVM_JIT_STRIP d)
#define FILC_LLVM_JIT_STRIP(kind, ...) kind, __VA_ARGS__
#define FILC_LLVM_JIT_UNWRAP_2(fname, kind, ...) \
    FILC_LLVM_JIT_UNWRAP_3(fname, kind, ##__VA_ARGS__)
#define FILC_LLVM_JIT_UNWRAP_3(fname, kind, ...) \
    FILC_LLVM_JIT_UNWRAP_##kind(fname, ##__VA_ARGS__)
#define FILC_LLVM_JIT_UNWRAP_CTX(fname, name) \
    filc_llvm_jit_context_object* name##_obj = unwrap_context(my_thread, name); \
    llvm_context = name##_obj;
#define FILC_LLVM_JIT_UNWRAP_REF(fname, name, kind, ref_ty, ...) \
    filc_llvm_jit_object* name##_obj = unwrap_obj( \
        my_thread, name, FILC_LLVM_JIT_KIND_##kind, &llvm_context, &llvm_module);
#define FILC_LLVM_JIT_UNWRAP_CSTR(fname, name) \
    const char* name##_obj = filc_check_and_get_tmp_str(my_thread, name);
#define FILC_LLVM_JIT_UNWRAP_OPTSTR(fname, name, len_name) \
    const char* name##_obj; \
    if (!filc_ptr_ptr(name)) { \
        FILC_CHECK(len_name == 0, NULL, "Fil-C LLVM JIT API: " #fname \
                   ": null string with nonzero length."); \
        name##_obj = NULL; \
    } else { \
        filc_check_read(name, (size_t)len_name); \
        name##_obj = (const char*)filc_ptr_ptr(name); \
    }
/* SCAL parameters are passed through unchanged; they unwrap to nothing. */
#define FILC_LLVM_JIT_UNWRAP_SCAL(fname, name, type)
#define FILC_LLVM_JIT_UNWRAP_ARR(fname, name, count_name, kind, elem_ty, ...) \
    elem_ty* name##_obj = (elem_ty*)decode_handle_array( \
        my_thread, name, count_name, FILC_LLVM_JIT_KIND_##kind, \
        &llvm_context, &llvm_module);


#define FILC_LLVM_JIT_ADOPT(d) FILC_LLVM_JIT_ADOPT_I d
#define FILC_LLVM_JIT_ADOPT_I(kind, builder) \
    if (llvm_module) \
        builder_adopt_module(my_thread, builder##_obj, llvm_module);

/* Locking discipline.  Context-less wrappers run lock-free: there is no
 * context to lock, and historically some wrappers locked &context->lock
 * even when context was provably NULL. */
#define FILC_LLVM_JIT_DECLS_LOCKED \
    filc_llvm_jit_context_object* llvm_context = NULL; \
    filc_llvm_jit_object* llvm_module = NULL;
#define FILC_LLVM_JIT_DECLS_LOCK_IF_CONTEXT FILC_LLVM_JIT_DECLS_LOCKED
#define FILC_LLVM_JIT_DECLS_NO_LOCK
#define FILC_LLVM_JIT_DECLS(lock) FILC_LLVM_JIT_DECLS_##lock

#define FILC_LLVM_JIT_LOCK_LOCKED \
    pas_lock_lock(&llvm_context->lock);
#define FILC_LLVM_JIT_LOCK_LOCK_IF_CONTEXT \
    if (llvm_context) { \
        pas_lock_lock(&llvm_context->lock); \
    }
#define FILC_LLVM_JIT_LOCK_NO_LOCK
#define FILC_LLVM_JIT_LOCK(lock) FILC_LLVM_JIT_LOCK_##lock

#define FILC_LLVM_JIT_UNLOCK_LOCKED \
    pas_lock_unlock(&llvm_context->lock);
#define FILC_LLVM_JIT_UNLOCK_LOCK_IF_CONTEXT \
    if (llvm_context) { \
        pas_lock_unlock(&llvm_context->lock); \
    }
#define FILC_LLVM_JIT_UNLOCK_NO_LOCK
#define FILC_LLVM_JIT_UNLOCK(lock) FILC_LLVM_JIT_UNLOCK_##lock

/* Scalar return for zero-descriptor wrappers; the ones with descriptors
 * inline their lock, validate, call, and unlock so that the validation stage
 * runs under the lock. */
#define FILC_LLVM_JIT_SCALAR_RETURN_LOCKED(name, ret_ty, args) \
    pas_lock_lock(&llvm_context->lock); \
    ret_ty result = (ret_ty)api->name args; \
    pas_lock_unlock(&llvm_context->lock); \
    return result;
#define FILC_LLVM_JIT_SCALAR_RETURN_NO_LOCK(name, ret_ty, args) \
    return (ret_ty)api->name args;

/* The DEFINE macros are variadic: a row may carry one trailing semantic-check
 * clause (a bare helper invocation, see the semantic-check section), which
 * expands inside the locked region, after VALIDATE and before the api-> call. */

#define FILC_LLVM_JIT_DEFINE_VOID_1(name, lock, d0, ...) \
void filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    api->name(FILC_LLVM_JIT_ARG(d0)); \
    FILC_LLVM_JIT_UNLOCK(lock) \
}

#define FILC_LLVM_JIT_DEFINE_VOID_2(name, lock, d0, d1, ...) \
void filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    api->name(FILC_LLVM_JIT_ARG(d0), FILC_LLVM_JIT_ARG(d1)); \
    FILC_LLVM_JIT_UNLOCK(lock) \
}

#define FILC_LLVM_JIT_DEFINE_VOID_3(name, lock, d0, d1, d2, ...) \
void filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1), FILC_LLVM_JIT_PARAM(d2)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_UNWRAP(name, d2) \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_VALIDATE(name, d2) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    api->name(FILC_LLVM_JIT_ARG(d0), FILC_LLVM_JIT_ARG(d1), FILC_LLVM_JIT_ARG(d2)); \
    FILC_LLVM_JIT_UNLOCK(lock) \
}

#define FILC_LLVM_JIT_DEFINE_VOID_4(name, lock, d0, d1, d2, d3, ...) \
void filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1), FILC_LLVM_JIT_PARAM(d2), FILC_LLVM_JIT_PARAM(d3)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_UNWRAP(name, d2) \
    FILC_LLVM_JIT_UNWRAP(name, d3) \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_VALIDATE(name, d2) \
    FILC_LLVM_JIT_VALIDATE(name, d3) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    api->name(FILC_LLVM_JIT_ARG(d0), FILC_LLVM_JIT_ARG(d1), FILC_LLVM_JIT_ARG(d2), FILC_LLVM_JIT_ARG(d3)); \
    FILC_LLVM_JIT_UNLOCK(lock) \
}

#define FILC_LLVM_JIT_DEFINE_VOID_5(name, lock, d0, d1, d2, d3, d4, ...) \
void filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1), FILC_LLVM_JIT_PARAM(d2), FILC_LLVM_JIT_PARAM(d3), FILC_LLVM_JIT_PARAM(d4)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_UNWRAP(name, d2) \
    FILC_LLVM_JIT_UNWRAP(name, d3) \
    FILC_LLVM_JIT_UNWRAP(name, d4) \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_VALIDATE(name, d2) \
    FILC_LLVM_JIT_VALIDATE(name, d3) \
    FILC_LLVM_JIT_VALIDATE(name, d4) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    api->name(FILC_LLVM_JIT_ARG(d0), FILC_LLVM_JIT_ARG(d1), FILC_LLVM_JIT_ARG(d2), FILC_LLVM_JIT_ARG(d3), FILC_LLVM_JIT_ARG(d4)); \
    FILC_LLVM_JIT_UNLOCK(lock) \
}

#define FILC_LLVM_JIT_DEFINE_VOID_2_ADOPT(name, lock, d0, d1, adopt, ...) \
void filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    api->name(FILC_LLVM_JIT_ARG(d0), FILC_LLVM_JIT_ARG(d1)); \
    FILC_LLVM_JIT_ADOPT(adopt) \
    FILC_LLVM_JIT_UNLOCK(lock) \
}

#define FILC_LLVM_JIT_DEFINE_VOID_3_ADOPT(name, lock, d0, d1, d2, adopt, ...) \
void filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1), FILC_LLVM_JIT_PARAM(d2)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_UNWRAP(name, d2) \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_VALIDATE(name, d2) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    api->name(FILC_LLVM_JIT_ARG(d0), FILC_LLVM_JIT_ARG(d1), FILC_LLVM_JIT_ARG(d2)); \
    FILC_LLVM_JIT_ADOPT(adopt) \
    FILC_LLVM_JIT_UNLOCK(lock) \
}

#define FILC_LLVM_JIT_DEFINE_REF_1(name, lock, kind, d0, ...) \
filc_ptr filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    void* result_raw; \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    result_raw = api->name(FILC_LLVM_JIT_ARG(d0)); \
    filc_ptr result = wrap_handle( \
        my_thread, llvm_context, llvm_module, FILC_LLVM_JIT_KIND_##kind, \
        result_raw); \
    FILC_LLVM_JIT_UNLOCK(lock) \
    return result; \
}

/* Fil-C: LLVMGetGlobalParent is hand-written (see its definition next to the
 * module rows below): the value's wrapper already points at the module object
 * that is the parent, so the parent is returned as THAT wrapper rather than
 * wrapped anew. */
#define FILC_LLVM_JIT_DEFINE_REF_2(name, lock, kind, d0, d1, ...) \
filc_ptr filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    void* result_raw; \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    result_raw = api->name(FILC_LLVM_JIT_ARG(d0), FILC_LLVM_JIT_ARG(d1)); \
    filc_ptr result = wrap_handle( \
        my_thread, llvm_context, llvm_module, FILC_LLVM_JIT_KIND_##kind, \
        result_raw); \
    FILC_LLVM_JIT_UNLOCK(lock) \
    return result; \
}

#define FILC_LLVM_JIT_DEFINE_REF_3(name, lock, kind, d0, d1, d2, ...) \
filc_ptr filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1), FILC_LLVM_JIT_PARAM(d2)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_UNWRAP(name, d2) \
    void* result_raw; \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_VALIDATE(name, d2) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    result_raw = api->name(FILC_LLVM_JIT_ARG(d0), FILC_LLVM_JIT_ARG(d1), FILC_LLVM_JIT_ARG(d2)); \
    filc_ptr result = wrap_handle( \
        my_thread, llvm_context, llvm_module, FILC_LLVM_JIT_KIND_##kind, \
        result_raw); \
    FILC_LLVM_JIT_UNLOCK(lock) \
    return result; \
}

#define FILC_LLVM_JIT_DEFINE_REF_4(name, lock, kind, d0, d1, d2, d3, ...) \
filc_ptr filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1), FILC_LLVM_JIT_PARAM(d2), FILC_LLVM_JIT_PARAM(d3)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_UNWRAP(name, d2) \
    FILC_LLVM_JIT_UNWRAP(name, d3) \
    void* result_raw; \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_VALIDATE(name, d2) \
    FILC_LLVM_JIT_VALIDATE(name, d3) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    result_raw = api->name(FILC_LLVM_JIT_ARG(d0), FILC_LLVM_JIT_ARG(d1), FILC_LLVM_JIT_ARG(d2), FILC_LLVM_JIT_ARG(d3)); \
    filc_ptr result = wrap_handle( \
        my_thread, llvm_context, llvm_module, FILC_LLVM_JIT_KIND_##kind, \
        result_raw); \
    FILC_LLVM_JIT_UNLOCK(lock) \
    return result; \
}

#define FILC_LLVM_JIT_DEFINE_REF_5(name, lock, kind, d0, d1, d2, d3, d4, ...) \
filc_ptr filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1), FILC_LLVM_JIT_PARAM(d2), FILC_LLVM_JIT_PARAM(d3), FILC_LLVM_JIT_PARAM(d4)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_UNWRAP(name, d2) \
    FILC_LLVM_JIT_UNWRAP(name, d3) \
    FILC_LLVM_JIT_UNWRAP(name, d4) \
    void* result_raw; \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_VALIDATE(name, d2) \
    FILC_LLVM_JIT_VALIDATE(name, d3) \
    FILC_LLVM_JIT_VALIDATE(name, d4) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    result_raw = api->name(FILC_LLVM_JIT_ARG(d0), FILC_LLVM_JIT_ARG(d1), FILC_LLVM_JIT_ARG(d2), FILC_LLVM_JIT_ARG(d3), FILC_LLVM_JIT_ARG(d4)); \
    filc_ptr result = wrap_handle( \
        my_thread, llvm_context, llvm_module, FILC_LLVM_JIT_KIND_##kind, \
        result_raw); \
    FILC_LLVM_JIT_UNLOCK(lock) \
    return result; \
}

#define FILC_LLVM_JIT_DEFINE_REF_6(name, lock, kind, d0, d1, d2, d3, d4, d5, ...) \
filc_ptr filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1), FILC_LLVM_JIT_PARAM(d2), FILC_LLVM_JIT_PARAM(d3), FILC_LLVM_JIT_PARAM(d4), FILC_LLVM_JIT_PARAM(d5)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_UNWRAP(name, d2) \
    FILC_LLVM_JIT_UNWRAP(name, d3) \
    FILC_LLVM_JIT_UNWRAP(name, d4) \
    FILC_LLVM_JIT_UNWRAP(name, d5) \
    void* result_raw; \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_VALIDATE(name, d2) \
    FILC_LLVM_JIT_VALIDATE(name, d3) \
    FILC_LLVM_JIT_VALIDATE(name, d4) \
    FILC_LLVM_JIT_VALIDATE(name, d5) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    result_raw = api->name(FILC_LLVM_JIT_ARG(d0), FILC_LLVM_JIT_ARG(d1), FILC_LLVM_JIT_ARG(d2), FILC_LLVM_JIT_ARG(d3), FILC_LLVM_JIT_ARG(d4), FILC_LLVM_JIT_ARG(d5)); \
    filc_ptr result = wrap_handle( \
        my_thread, llvm_context, llvm_module, FILC_LLVM_JIT_KIND_##kind, \
        result_raw); \
    FILC_LLVM_JIT_UNLOCK(lock) \
    return result; \
}

#define FILC_LLVM_JIT_DEFINE_REF_7(name, lock, kind, d0, d1, d2, d3, d4, d5, d6, ...) \
filc_ptr filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1), FILC_LLVM_JIT_PARAM(d2), FILC_LLVM_JIT_PARAM(d3), FILC_LLVM_JIT_PARAM(d4), FILC_LLVM_JIT_PARAM(d5), FILC_LLVM_JIT_PARAM(d6)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_UNWRAP(name, d2) \
    FILC_LLVM_JIT_UNWRAP(name, d3) \
    FILC_LLVM_JIT_UNWRAP(name, d4) \
    FILC_LLVM_JIT_UNWRAP(name, d5) \
    FILC_LLVM_JIT_UNWRAP(name, d6) \
    void* result_raw; \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_VALIDATE(name, d2) \
    FILC_LLVM_JIT_VALIDATE(name, d3) \
    FILC_LLVM_JIT_VALIDATE(name, d4) \
    FILC_LLVM_JIT_VALIDATE(name, d5) \
    FILC_LLVM_JIT_VALIDATE(name, d6) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    result_raw = api->name(FILC_LLVM_JIT_ARG(d0), FILC_LLVM_JIT_ARG(d1), FILC_LLVM_JIT_ARG(d2), FILC_LLVM_JIT_ARG(d3), FILC_LLVM_JIT_ARG(d4), FILC_LLVM_JIT_ARG(d5), FILC_LLVM_JIT_ARG(d6)); \
    filc_ptr result = wrap_handle( \
        my_thread, llvm_context, llvm_module, FILC_LLVM_JIT_KIND_##kind, \
        result_raw); \
    FILC_LLVM_JIT_UNLOCK(lock) \
    return result; \
}

#define FILC_LLVM_JIT_DEFINE_REF_8(name, lock, kind, d0, d1, d2, d3, d4, d5, d6, d7, ...) \
filc_ptr filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1), FILC_LLVM_JIT_PARAM(d2), FILC_LLVM_JIT_PARAM(d3), FILC_LLVM_JIT_PARAM(d4), FILC_LLVM_JIT_PARAM(d5), FILC_LLVM_JIT_PARAM(d6), FILC_LLVM_JIT_PARAM(d7)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_UNWRAP(name, d2) \
    FILC_LLVM_JIT_UNWRAP(name, d3) \
    FILC_LLVM_JIT_UNWRAP(name, d4) \
    FILC_LLVM_JIT_UNWRAP(name, d5) \
    FILC_LLVM_JIT_UNWRAP(name, d6) \
    FILC_LLVM_JIT_UNWRAP(name, d7) \
    void* result_raw; \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_VALIDATE(name, d2) \
    FILC_LLVM_JIT_VALIDATE(name, d3) \
    FILC_LLVM_JIT_VALIDATE(name, d4) \
    FILC_LLVM_JIT_VALIDATE(name, d5) \
    FILC_LLVM_JIT_VALIDATE(name, d6) \
    FILC_LLVM_JIT_VALIDATE(name, d7) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    result_raw = api->name(FILC_LLVM_JIT_ARG(d0), FILC_LLVM_JIT_ARG(d1), FILC_LLVM_JIT_ARG(d2), FILC_LLVM_JIT_ARG(d3), FILC_LLVM_JIT_ARG(d4), FILC_LLVM_JIT_ARG(d5), FILC_LLVM_JIT_ARG(d6), FILC_LLVM_JIT_ARG(d7)); \
    filc_ptr result = wrap_handle( \
        my_thread, llvm_context, llvm_module, FILC_LLVM_JIT_KIND_##kind, \
        result_raw); \
    FILC_LLVM_JIT_UNLOCK(lock) \
    return result; \
}

#define FILC_LLVM_JIT_DEFINE_REF_9(name, lock, kind, d0, d1, d2, d3, d4, d5, d6, d7, d8, ...) \
filc_ptr filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1), FILC_LLVM_JIT_PARAM(d2), FILC_LLVM_JIT_PARAM(d3), FILC_LLVM_JIT_PARAM(d4), FILC_LLVM_JIT_PARAM(d5), FILC_LLVM_JIT_PARAM(d6), FILC_LLVM_JIT_PARAM(d7), FILC_LLVM_JIT_PARAM(d8)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_UNWRAP(name, d2) \
    FILC_LLVM_JIT_UNWRAP(name, d3) \
    FILC_LLVM_JIT_UNWRAP(name, d4) \
    FILC_LLVM_JIT_UNWRAP(name, d5) \
    FILC_LLVM_JIT_UNWRAP(name, d6) \
    FILC_LLVM_JIT_UNWRAP(name, d7) \
    FILC_LLVM_JIT_UNWRAP(name, d8) \
    void* result_raw; \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_VALIDATE(name, d2) \
    FILC_LLVM_JIT_VALIDATE(name, d3) \
    FILC_LLVM_JIT_VALIDATE(name, d4) \
    FILC_LLVM_JIT_VALIDATE(name, d5) \
    FILC_LLVM_JIT_VALIDATE(name, d6) \
    FILC_LLVM_JIT_VALIDATE(name, d7) \
    FILC_LLVM_JIT_VALIDATE(name, d8) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    result_raw = api->name(FILC_LLVM_JIT_ARG(d0), FILC_LLVM_JIT_ARG(d1), FILC_LLVM_JIT_ARG(d2), FILC_LLVM_JIT_ARG(d3), FILC_LLVM_JIT_ARG(d4), FILC_LLVM_JIT_ARG(d5), FILC_LLVM_JIT_ARG(d6), FILC_LLVM_JIT_ARG(d7), FILC_LLVM_JIT_ARG(d8)); \
    filc_ptr result = wrap_handle( \
        my_thread, llvm_context, llvm_module, FILC_LLVM_JIT_KIND_##kind, \
        result_raw); \
    FILC_LLVM_JIT_UNLOCK(lock) \
    return result; \
}

#define FILC_LLVM_JIT_DEFINE_SCALAR_0(name, lock, ret_ty) \
ret_ty filc_native_##name(filc_thread* my_thread) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_SCALAR_RETURN_##lock(name, ret_ty, ()) \
}

#define FILC_LLVM_JIT_DEFINE_SCALAR_1(name, lock, ret_ty, d0, ...) \
ret_ty filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    ret_ty result = (ret_ty)api->name(FILC_LLVM_JIT_ARG(d0)); \
    FILC_LLVM_JIT_UNLOCK(lock) \
    return result; \
}

#define FILC_LLVM_JIT_DEFINE_SCALAR_2(name, lock, ret_ty, d0, d1, ...) \
ret_ty filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    ret_ty result = (ret_ty)api->name(FILC_LLVM_JIT_ARG(d0),FILC_LLVM_JIT_ARG(d1)); \
    FILC_LLVM_JIT_UNLOCK(lock) \
    return result; \
}

#define FILC_LLVM_JIT_DEFINE_SCALAR_3(name, lock, ret_ty, d0, d1, d2, ...) \
ret_ty filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1), FILC_LLVM_JIT_PARAM(d2)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_UNWRAP(name, d2) \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_VALIDATE(name, d2) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    ret_ty result = (ret_ty)api->name(FILC_LLVM_JIT_ARG(d0),FILC_LLVM_JIT_ARG(d1),FILC_LLVM_JIT_ARG(d2)); \
    FILC_LLVM_JIT_UNLOCK(lock) \
    return result; \
}

#define FILC_LLVM_JIT_DEFINE_SCALAR_4(name, lock, ret_ty, d0, d1, d2, d3, ...) \
ret_ty filc_native_##name(filc_thread* my_thread, FILC_LLVM_JIT_PARAM(d0), FILC_LLVM_JIT_PARAM(d1), FILC_LLVM_JIT_PARAM(d2), FILC_LLVM_JIT_PARAM(d3)) \
{ \
    ensure_api(my_thread); \
    FILC_LLVM_JIT_DECLS(lock) \
    FILC_LLVM_JIT_UNWRAP(name, d0) \
    FILC_LLVM_JIT_UNWRAP(name, d1) \
    FILC_LLVM_JIT_UNWRAP(name, d2) \
    FILC_LLVM_JIT_UNWRAP(name, d3) \
    FILC_LLVM_JIT_LOCK(lock) \
    FILC_LLVM_JIT_VALIDATE(name, d0) \
    FILC_LLVM_JIT_VALIDATE(name, d1) \
    FILC_LLVM_JIT_VALIDATE(name, d2) \
    FILC_LLVM_JIT_VALIDATE(name, d3) \
    FILC_LLVM_JIT_SEM(name, ##__VA_ARGS__) \
    ret_ty result = (ret_ty)api->name(FILC_LLVM_JIT_ARG(d0),FILC_LLVM_JIT_ARG(d1),FILC_LLVM_JIT_ARG(d2),FILC_LLVM_JIT_ARG(d3)); \
    FILC_LLVM_JIT_UNLOCK(lock) \
    return result; \
}

/* The wrapper table, one row per wrapper, in alphabetical order. */

FILC_LLVM_JIT_DEFINE_REF_5(LLVMAddAlias2, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef), (REF, value_ty, TYPE, LLVMTypeRef), (SCAL, addr_space, unsigned), (REF, aliasee, VALUE, LLVMValueRef, Constant), (CSTR, name), sem_alias_value_ty(LLVMAddAlias2, value_ty, addr_space, aliasee))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMAddAttributeAtIndex, LOCKED, (REF, value, VALUE, LLVMValueRef, Function), (SCAL, idx, unsigned), (REF, attribute, ATTRIBUTE, LLVMAttributeRef))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMAddCallSiteAttribute, LOCKED, (REF, value, VALUE, LLVMValueRef, CallBase), (SCAL, idx, unsigned), (REF, attribute, ATTRIBUTE, LLVMAttributeRef))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMAddCase, LOCKED, (REF, switch_inst, VALUE, LLVMValueRef, SwitchInst), (REF, on_val, VALUE, LLVMValueRef, ConstantInt), (REF, dest, BASIC_BLOCK, LLVMBasicBlockRef), sem_inst_bb(LLVMAddCase, switch_inst, dest))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMAddClause, LOCKED, (REF, landing_pad, VALUE, LLVMValueRef, LandingPadInst), (REF, clause_val, VALUE, LLVMValueRef, Constant))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMAddDestination, LOCKED, (REF, indirect_br, VALUE, LLVMValueRef, IndirectBrInst), (REF, dest, BASIC_BLOCK, LLVMBasicBlockRef), sem_inst_bb(LLVMAddDestination, indirect_br, dest))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMAddFunction, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef), (CSTR, name), (REF, function_ty, TYPE, LLVMTypeRef, FunctionTypeKind))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMAddGlobal, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef), (REF, type, TYPE, LLVMTypeRef), (CSTR, name), sem_global_ty(LLVMAddGlobal, type))
FILC_LLVM_JIT_DEFINE_REF_6(LLVMAddGlobalIFunc, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef), (OPTSTR, name, name_len), (SCAL, name_len, size_t), (REF, type, TYPE, LLVMTypeRef), (SCAL, addr_space, unsigned), (REF, resolver, VALUE, LLVMValueRef, Constant), sem_address_space(LLVMAddGlobalIFunc, addr_space))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMAddGlobalInAddressSpace, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef), (REF, type, TYPE, LLVMTypeRef), (CSTR, name), (SCAL, address_space, unsigned), sem_global_ty(LLVMAddGlobalInAddressSpace, type) sem_address_space(LLVMAddGlobalInAddressSpace, address_space))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMAddHandler, LOCKED, (REF, catch_switch, VALUE, LLVMValueRef, CatchSwitchInst), (REF, dest, BASIC_BLOCK, LLVMBasicBlockRef), sem_inst_bb(LLVMAddHandler, catch_switch, dest))
FILC_LLVM_JIT_DEFINE_VOID_4(LLVMAddIncoming, LOCKED, (REF, phi_node, VALUE, LLVMValueRef, PHINode), (ARR, incoming_values, count, VALUE, LLVMValueRef), (ARR, incoming_blocks, count, BASIC_BLOCK, LLVMBasicBlockRef), (SCAL, count, unsigned), sem_add_incoming(LLVMAddIncoming, phi_node, incoming_values, incoming_blocks, count))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMAddMetadataToInst, LOCKED, (REF, builder, BUILDER, LLVMBuilderRef), (REF, inst, VALUE, LLVMValueRef, Instruction))
FILC_LLVM_JIT_DEFINE_VOID_5(LLVMAddModuleFlag, LOCKED, (REF, module, MODULE, LLVMModuleRef), (SCAL, behavior, unsigned), (OPTSTR, key, key_len), (SCAL, key_len, size_t), (REF, val, METADATA, LLVMMetadataRef), sem_module_flag_behavior(LLVMAddModuleFlag, behavior))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMAddNamedMetadataOperand, LOCKED, (REF, module, MODULE, LLVMModuleRef), (CSTR, name), (REF, val, VALUE, LLVMValueRef, ExtractMDNodeValue))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMAddTargetDependentFunctionAttr, LOCKED, (REF, function, VALUE, LLVMValueRef, Function), (CSTR, name), (CSTR, value))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMAliasGetAliasee, LOCKED, VALUE, (REF, alias, VALUE, LLVMValueRef, GlobalAlias))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMAliasSetAliasee, LOCKED, (REF, alias, VALUE, LLVMValueRef, GlobalAlias), (REF, aliasee, VALUE, LLVMValueRef, Constant), sem_alias_aliasee(LLVMAliasSetAliasee, alias, aliasee))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMAlignOf, LOCKED, VALUE, (REF, type, TYPE, LLVMTypeRef))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMAppendBasicBlockInContext, LOCKED, BASIC_BLOCK, (CTX, context), (REF, function, VALUE, LLVMValueRef, Function), (CSTR, name))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMAppendExistingBasicBlock, LOCKED, (REF, function, VALUE, LLVMValueRef, Function), (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef), sem_unparented_bb(LLVMAppendExistingBasicBlock, basic_block))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMAppendModuleInlineAsm, LOCKED, (REF, module, MODULE, LLVMModuleRef), (OPTSTR, asm_string, len), (SCAL, len, size_t))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMArrayType, LOCKED, TYPE, (REF, element_type, TYPE, LLVMTypeRef), (SCAL, element_count, unsigned), sem_array_elem_ty(LLVMArrayType, element_type))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMArrayType2, LOCKED, TYPE, (REF, element_type, TYPE, LLVMTypeRef), (SCAL, element_count, unsigned long long), sem_array_elem_ty(LLVMArrayType2, element_type))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMBFloatTypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMBasicBlockAsValue, LOCKED, VALUE, (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMBlockAddress, LOCKED, VALUE, (REF, value, VALUE, LLVMValueRef, Function), (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildAShr, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildAShr, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildAdd, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildAdd, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildAddrSpaceCast, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_cast(LLVMBuildAddrSpaceCast, LLVMAddrSpaceCast, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildAggregateRet, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (ARR, ret_vals, num_elements, VALUE, LLVMValueRef), (SCAL, num_elements, unsigned), sem_aggregate_ret(LLVMBuildAggregateRet, builder))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildAlloca, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, type, TYPE, LLVMTypeRef), (CSTR, name), sem_insert_position(LLVMBuildAlloca, builder) sem_sized(LLVMBuildAlloca, type))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildAnd, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildAnd, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildArrayAlloca, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, type, TYPE, LLVMTypeRef), (REF, val, VALUE, LLVMValueRef), (CSTR, name), sem_insert_position(LLVMBuildArrayAlloca, builder) sem_array_alloc(LLVMBuildArrayAlloca, type, val))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildArrayMalloc, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, type, TYPE, LLVMTypeRef), (REF, val, VALUE, LLVMValueRef), (CSTR, name), sem_insert_position(LLVMBuildArrayMalloc, builder) sem_array_alloc(LLVMBuildArrayMalloc, type, val))
FILC_LLVM_JIT_DEFINE_REF_7(LLVMBuildAtomicCmpXchg, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, ptr, VALUE, LLVMValueRef), (REF, cmp, VALUE, LLVMValueRef), (REF, new, VALUE, LLVMValueRef), (SCAL, success_ordering, unsigned), (SCAL, failure_ordering, unsigned), (SCAL, single_thread, bool), sem_insert_position(LLVMBuildAtomicCmpXchg, builder) sem_atomic_cmpxchg(LLVMBuildAtomicCmpXchg, ptr, cmp, new, success_ordering, failure_ordering))
FILC_LLVM_JIT_DEFINE_REF_7(LLVMBuildAtomicCmpXchgSyncScope, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, ptr, VALUE, LLVMValueRef), (REF, cmp, VALUE, LLVMValueRef), (REF, new, VALUE, LLVMValueRef), (SCAL, success_ordering, unsigned), (SCAL, failure_ordering, unsigned), (SCAL, ssid, unsigned), sem_insert_position(LLVMBuildAtomicCmpXchgSyncScope, builder) sem_atomic_cmpxchg(LLVMBuildAtomicCmpXchgSyncScope, ptr, cmp, new, success_ordering, failure_ordering))
FILC_LLVM_JIT_DEFINE_REF_6(LLVMBuildAtomicRMW, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (SCAL, bin_op, unsigned), (REF, ptr, VALUE, LLVMValueRef), (REF, val, VALUE, LLVMValueRef), (SCAL, ordering, unsigned), (SCAL, single_thread, bool), sem_insert_position(LLVMBuildAtomicRMW, builder) sem_atomic_rmw(LLVMBuildAtomicRMW, bin_op, ptr, ordering))
FILC_LLVM_JIT_DEFINE_REF_6(LLVMBuildAtomicRMWSyncScope, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (SCAL, bin_op, unsigned), (REF, ptr, VALUE, LLVMValueRef), (REF, val, VALUE, LLVMValueRef), (SCAL, ordering, unsigned), (SCAL, ssid, unsigned), sem_insert_position(LLVMBuildAtomicRMWSyncScope, builder) sem_atomic_rmw(LLVMBuildAtomicRMWSyncScope, bin_op, ptr, ordering))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildBinOp, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (SCAL, opcode, unsigned), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_binop(LLVMBuildBinOp, opcode, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildBitCast, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_cast(LLVMBuildBitCast, LLVMBitCast, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMBuildBr, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, dest, BASIC_BLOCK, LLVMBasicBlockRef), sem_builder_bb(LLVMBuildBr, builder, dest))
FILC_LLVM_JIT_DEFINE_REF_6(LLVMBuildCall2, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, type, TYPE, LLVMTypeRef, FunctionTypeKind), (REF, function, VALUE, LLVMValueRef), (ARR, args, num_args, VALUE, LLVMValueRef), (SCAL, num_args, unsigned), (CSTR, name), sem_call_sig(LLVMBuildCall2, type, num_args) sem_nameable_result(LLVMBuildCall2, type, name))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildCast, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (SCAL, opcode, unsigned), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_cast_op(LLVMBuildCast, opcode, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildCatchPad, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, parent_pad, VALUE, LLVMValueRef), (ARR, args, num_args, VALUE, LLVMValueRef), (SCAL, num_args, unsigned), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildCatchRet, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, catch_pad, VALUE, LLVMValueRef, CatchPadInst), (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef), sem_builder_bb(LLVMBuildCatchRet, builder, basic_block))
/* LLVMBuildCatchSwitch takes the parent pad without any type check:
 * CatchSwitchInst::init only asserts ParentPad && NumReservedValues
 * (Instructions.cpp), and IRBuilderBase::CreateCatchSwitch never looks at
 * the type, so LLVM silently accepts a non-token parent pad (probed: an i32
 * constant and a global variable are both accepted without an assertion).
 * No clarifying clause is added here on purpose: the suite's donors build
 * catchswitches with garbage parent pads as legal preconditions for OTHER
 * panics (jit_badtype_set_parent_catch_switch and
 * jit_badtype_set_parent_catch_switch_pad pass an i32 constant), so a
 * token-typed parent-pad requirement would be unsound.  The wrapper's own
 * object handling is what catches the bad directions: a null parent pad
 * trips the generic "cannot access pointer with null object" Fil-C safety
 * panic (before CatchSwitchInst::init's ParentPad assert could fire), and a
 * non-VALUE object trips the wrapper's "object kind mismatch" check.  Both
 * generic panics are the correct outcomes. */
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildCatchSwitch, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, parent_pad, VALUE, LLVMValueRef), (REF, unwind_bb, BASIC_BLOCK, LLVMBasicBlockRef), (SCAL, num_handlers, unsigned), (CSTR, name), sem_catchswitch_handlers(LLVMBuildCatchSwitch, num_handlers) sem_catchswitch_unwind(LLVMBuildCatchSwitch, builder, unwind_bb))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildCleanupPad, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, parent_pad, VALUE, LLVMValueRef), (ARR, args, num_args, VALUE, LLVMValueRef), (SCAL, num_args, unsigned), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildCleanupRet, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, catch_pad, VALUE, LLVMValueRef, CleanupPadInst), (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef), sem_builder_bb(LLVMBuildCleanupRet, builder, basic_block))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildCondBr, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, if_val, VALUE, LLVMValueRef), (REF, then, BASIC_BLOCK, LLVMBasicBlockRef), (REF, else_val, BASIC_BLOCK, LLVMBasicBlockRef), sem_cond_br(LLVMBuildCondBr, builder, if_val, then, else_val))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildExactSDiv, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildExactSDiv, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildExactUDiv, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildExactUDiv, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildExtractElement, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, vec_val, VALUE, LLVMValueRef), (REF, index, VALUE, LLVMValueRef), (CSTR, name), sem_extract_element(LLVMBuildExtractElement, vec_val, index))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildExtractValue, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, agg_val, VALUE, LLVMValueRef), (SCAL, index, unsigned), (CSTR, name), sem_extract_value(LLVMBuildExtractValue, agg_val, index))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildFAdd, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_fp_binop(LLVMBuildFAdd, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildFCmp, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (SCAL, predicate, unsigned), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_fcmp(LLVMBuildFCmp, predicate, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildFDiv, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_fp_binop(LLVMBuildFDiv, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildFMul, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_fp_binop(LLVMBuildFMul, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildFNeg, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, value, VALUE, LLVMValueRef), (CSTR, name), sem_fp_unop(LLVMBuildFNeg, value))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildFPCast, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_fp_cast(LLVMBuildFPCast, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildFPExt, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_cast(LLVMBuildFPExt, LLVMFPExt, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildFPToSI, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_cast(LLVMBuildFPToSI, LLVMFPToSI, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildFPToUI, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_cast(LLVMBuildFPToUI, LLVMFPToUI, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildFPTrunc, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_cast(LLVMBuildFPTrunc, LLVMFPTrunc, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildFRem, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_fp_binop(LLVMBuildFRem, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildFSub, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_fp_binop(LLVMBuildFSub, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildFence, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (SCAL, ordering, unsigned), (SCAL, single_thread, bool), (CSTR, name), sem_ordering(LLVMBuildFence, ordering) sem_unnamed(LLVMBuildFence, name))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildFenceSyncScope, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (SCAL, ordering, unsigned), (SCAL, ssid, unsigned), (CSTR, name), sem_ordering(LLVMBuildFenceSyncScope, ordering) sem_unnamed(LLVMBuildFenceSyncScope, name))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMBuildFree, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, pointer_val, VALUE, LLVMValueRef), sem_ptr_operand(LLVMBuildFree, pointer_val))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildFreeze, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (CSTR, name), sem_nameable_value(LLVMBuildFreeze, val, name))
FILC_LLVM_JIT_DEFINE_REF_6(LLVMBuildGEP2, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, type, TYPE, LLVMTypeRef), (REF, pointer, VALUE, LLVMValueRef), (ARR, indices, num_indices, VALUE, LLVMValueRef), (SCAL, num_indices, unsigned), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_7(LLVMBuildGEPWithNoWrapFlags, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, type, TYPE, LLVMTypeRef), (REF, pointer, VALUE, LLVMValueRef), (ARR, indices, num_indices, VALUE, LLVMValueRef), (SCAL, num_indices, unsigned), (CSTR, name), (SCAL, no_wrap_flags, unsigned))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildGlobalString, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (CSTR, str), (CSTR, name), sem_insert_position(LLVMBuildGlobalString, builder))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildGlobalStringPtr, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (CSTR, str), (CSTR, name), sem_insert_position(LLVMBuildGlobalStringPtr, builder))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildICmp, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (SCAL, predicate, unsigned), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_icmp(LLVMBuildICmp, predicate, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_6(LLVMBuildInBoundsGEP2, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, type, TYPE, LLVMTypeRef), (REF, pointer, VALUE, LLVMValueRef), (ARR, indices, num_indices, VALUE, LLVMValueRef), (SCAL, num_indices, unsigned), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildIndirectBr, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, addr, VALUE, LLVMValueRef), (SCAL, num_dests, unsigned), sem_ptr_operand(LLVMBuildIndirectBr, addr))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildInsertElement, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, vec_val, VALUE, LLVMValueRef), (REF, elt_val, VALUE, LLVMValueRef), (REF, index, VALUE, LLVMValueRef), (CSTR, name), sem_insert_element(LLVMBuildInsertElement, vec_val, elt_val, index))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildInsertValue, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, agg_val, VALUE, LLVMValueRef), (REF, elt_val, VALUE, LLVMValueRef), (SCAL, index, unsigned), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildIntCast, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_int_cast(LLVMBuildIntCast, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildIntCast2, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (SCAL, is_signed, bool), (CSTR, name), sem_int_cast(LLVMBuildIntCast2, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildIntToPtr, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_cast(LLVMBuildIntToPtr, LLVMIntToPtr, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_8(LLVMBuildInvoke2, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, type, TYPE, LLVMTypeRef, FunctionTypeKind), (REF, function, VALUE, LLVMValueRef), (ARR, args, num_args, VALUE, LLVMValueRef), (SCAL, num_args, unsigned), (REF, then, BASIC_BLOCK, LLVMBasicBlockRef), (REF, catch, BASIC_BLOCK, LLVMBasicBlockRef), (CSTR, name), sem_invoke(LLVMBuildInvoke2, builder, type, num_args, then, catch) sem_nameable_result(LLVMBuildInvoke2, type, name))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildIsNotNull, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildIsNull, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildLShr, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildLShr, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildLandingPad, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, type, TYPE, LLVMTypeRef), (REF, pers_fn, VALUE, LLVMValueRef, Function), (SCAL, num_clauses, unsigned), (CSTR, name), sem_insert_position(LLVMBuildLandingPad, builder) sem_nameable_ty(LLVMBuildLandingPad, type, name))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildLoad2, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, type, TYPE, LLVMTypeRef), (REF, pointer_val, VALUE, LLVMValueRef), (CSTR, name), sem_insert_position(LLVMBuildLoad2, builder) sem_load(LLVMBuildLoad2, type, pointer_val))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildMalloc, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, type, TYPE, LLVMTypeRef), (CSTR, name), sem_insert_position(LLVMBuildMalloc, builder) sem_sized(LLVMBuildMalloc, type))
FILC_LLVM_JIT_DEFINE_REF_6(LLVMBuildMemCpy, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, dst, VALUE, LLVMValueRef), (SCAL, dst_align, unsigned), (REF, src, VALUE, LLVMValueRef), (SCAL, src_align, unsigned), (REF, size, VALUE, LLVMValueRef), sem_insert_position(LLVMBuildMemCpy, builder) sem_mem_align(LLVMBuildMemCpy, dst_align) sem_mem_align(LLVMBuildMemCpy, src_align))
FILC_LLVM_JIT_DEFINE_REF_6(LLVMBuildMemMove, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, dst, VALUE, LLVMValueRef), (SCAL, dst_align, unsigned), (REF, src, VALUE, LLVMValueRef), (SCAL, src_align, unsigned), (REF, size, VALUE, LLVMValueRef), sem_insert_position(LLVMBuildMemMove, builder) sem_mem_align(LLVMBuildMemMove, dst_align) sem_mem_align(LLVMBuildMemMove, src_align))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildMemSet, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, ptr, VALUE, LLVMValueRef), (REF, val, VALUE, LLVMValueRef), (REF, len, VALUE, LLVMValueRef), (SCAL, align, unsigned), sem_insert_position(LLVMBuildMemSet, builder) sem_mem_align(LLVMBuildMemSet, align))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildMul, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildMul, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildNSWAdd, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildNSWAdd, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildNSWMul, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildNSWMul, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildNSWNeg, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, value, VALUE, LLVMValueRef), (CSTR, name), sem_int_unop(LLVMBuildNSWNeg, value))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildNSWSub, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildNSWSub, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildNUWAdd, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildNUWAdd, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildNUWMul, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildNUWMul, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildNUWSub, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildNUWSub, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildNeg, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, value, VALUE, LLVMValueRef), (CSTR, name), sem_int_unop(LLVMBuildNeg, value))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildNot, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, value, VALUE, LLVMValueRef), (CSTR, name), sem_int_unop(LLVMBuildNot, value))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildOr, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildOr, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildPhi, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, type, TYPE, LLVMTypeRef), (CSTR, name), sem_phi_ty(LLVMBuildPhi, type))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildPointerCast, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_pointer_cast(LLVMBuildPointerCast, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildPtrDiff2, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, elem_ty, TYPE, LLVMTypeRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_ptr_diff2(LLVMBuildPtrDiff2, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildPtrToInt, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_cast(LLVMBuildPtrToInt, LLVMPtrToInt, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMBuildResume, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, exn, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMBuildRet, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, value, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMBuildRetVoid, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildSDiv, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildSDiv, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildSExt, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_cast(LLVMBuildSExt, LLVMSExt, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildSExtOrBitCast, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_or_bit_cast(LLVMBuildSExtOrBitCast, LLVMSExt, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildSIToFP, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_cast(LLVMBuildSIToFP, LLVMSIToFP, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildSRem, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildSRem, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildSelect, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, if_val, VALUE, LLVMValueRef), (REF, then, VALUE, LLVMValueRef), (REF, else_val, VALUE, LLVMValueRef), (CSTR, name), sem_select(LLVMBuildSelect, if_val, then, else_val))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildShl, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildShl, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildShuffleVector, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, v1, VALUE, LLVMValueRef), (REF, v2, VALUE, LLVMValueRef), (REF, mask, VALUE, LLVMValueRef), (CSTR, name), sem_shuffle_vector(LLVMBuildShuffleVector, v1, v2, mask))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMBuildStore, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, ptr, VALUE, LLVMValueRef), sem_insert_position(LLVMBuildStore, builder) sem_store_value(LLVMBuildStore, val) sem_ptr_operand(LLVMBuildStore, ptr))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMBuildStructGEP2, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, type, TYPE, LLVMTypeRef), (REF, pointer, VALUE, LLVMValueRef), (SCAL, idx, unsigned), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildSub, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildSub, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildSwitch, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, value, VALUE, LLVMValueRef), (REF, else_val, BASIC_BLOCK, LLVMBasicBlockRef), (SCAL, num_cases, unsigned), sem_builder_bb(LLVMBuildSwitch, builder, else_val))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildTrunc, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_cast(LLVMBuildTrunc, LLVMTrunc, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildTruncOrBitCast, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_or_bit_cast(LLVMBuildTruncOrBitCast, LLVMTrunc, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildUDiv, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildUDiv, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildUIToFP, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_cast(LLVMBuildUIToFP, LLVMUIToFP, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildURem, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildURem, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMBuildUnreachable, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildVAArg, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, list, VALUE, LLVMValueRef), (REF, type, TYPE, LLVMTypeRef), (CSTR, name), sem_va_arg(LLVMBuildVAArg, list, type))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildXor, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, lhs, VALUE, LLVMValueRef), (REF, rhs, VALUE, LLVMValueRef), (CSTR, name), sem_int_binop(LLVMBuildXor, lhs, rhs))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildZExt, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_cast(LLVMBuildZExt, LLVMZExt, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMBuildZExtOrBitCast, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef), (REF, val, VALUE, LLVMValueRef), (REF, dest_ty, TYPE, LLVMTypeRef), (CSTR, name), sem_or_bit_cast(LLVMBuildZExtOrBitCast, LLVMZExt, val, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMBuilderGetDefaultFPMathTag, LOCKED, METADATA, (REF, builder, BUILDER, LLVMBuilderRef))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMBuilderSetDefaultFPMathTag, LOCKED, (REF, builder, BUILDER, LLVMBuilderRef), (REF, fp_math_tag, METADATA, LLVMMetadataRef, MDNodeMetadata))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMCanValueUseFastMathFlags, LOCKED, bool, (REF, inst, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_VOID_1(LLVMClearInsertionPosition, LOCKED, (REF, builder, BUILDER, LLVMBuilderRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMCloneModule, LOCKED, MODULE, (REF, module, MODULE, LLVMModuleRef))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstAdd, LOCKED, VALUE, (REF, lhs_constant, VALUE, LLVMValueRef, Constant), (REF, rhs_constant, VALUE, LLVMValueRef, Constant), sem_int_binop(LLVMConstAdd, lhs_constant, rhs_constant))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstAddrSpaceCast, LOCKED, VALUE, (REF, constant_val, VALUE, LLVMValueRef, Constant), (REF, to_type, TYPE, LLVMTypeRef), sem_const_addr_space_cast(LLVMConstAddrSpaceCast, constant_val, to_type))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMConstAllOnes, LOCKED, VALUE, (REF, type, TYPE, LLVMTypeRef), sem_const_all_ones(LLVMConstAllOnes, type))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMConstArray, LOCKED, VALUE, (REF, element_ty, TYPE, LLVMTypeRef), (ARR, constant_vals, length, VALUE, LLVMValueRef, Constant), (SCAL, length, unsigned), sem_const_array(LLVMConstArray, element_ty, constant_vals, length))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMConstArray2, LOCKED, VALUE, (REF, element_ty, TYPE, LLVMTypeRef), (ARR, constant_vals, length, VALUE, LLVMValueRef, Constant), (SCAL, length, unsigned long long), sem_const_array(LLVMConstArray2, element_ty, constant_vals, length))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstBitCast, LOCKED, VALUE, (REF, constant_val, VALUE, LLVMValueRef, Constant), (REF, to_type, TYPE, LLVMTypeRef), sem_const_bit_cast(LLVMConstBitCast, constant_val, to_type))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstExtractElement, LOCKED, VALUE, (REF, vector_constant, VALUE, LLVMValueRef, Constant), (REF, index_constant, VALUE, LLVMValueRef, Constant), sem_const_extract_element(LLVMConstExtractElement, vector_constant, index_constant))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMConstGEP2, LOCKED, VALUE, (REF, type, TYPE, LLVMTypeRef), (REF, constant_val, VALUE, LLVMValueRef, Constant), (ARR, constant_indices, num_indices, VALUE, LLVMValueRef, Constant), (SCAL, num_indices, unsigned), sem_const_gep(LLVMConstGEP2, type, constant_indices, num_indices))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMConstGEPWithNoWrapFlags, LOCKED, VALUE, (REF, type, TYPE, LLVMTypeRef), (REF, constant_val, VALUE, LLVMValueRef, Constant), (ARR, constant_indices, num_indices, VALUE, LLVMValueRef, Constant), (SCAL, num_indices, unsigned), (SCAL, no_wrap_flags, unsigned), sem_const_gep(LLVMConstGEPWithNoWrapFlags, type, constant_indices, num_indices))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMConstInBoundsGEP2, LOCKED, VALUE, (REF, type, TYPE, LLVMTypeRef), (REF, constant_val, VALUE, LLVMValueRef, Constant), (ARR, constant_indices, num_indices, VALUE, LLVMValueRef, Constant), (SCAL, num_indices, unsigned), sem_const_gep(LLVMConstInBoundsGEP2, type, constant_indices, num_indices))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMConstInlineAsm, LOCKED, VALUE, (REF, type, TYPE, LLVMTypeRef, FunctionTypeKind), (CSTR, asm_string), (CSTR, constraints), (SCAL, has_side_effects, bool), (SCAL, is_align_stack, bool), sem_inline_asm_cstr(LLVMConstInlineAsm, type, constraints))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMConstInsertElement, LOCKED, VALUE, (REF, vector_constant, VALUE, LLVMValueRef, Constant), (REF, element_value_constant, VALUE, LLVMValueRef, Constant), (REF, index_constant, VALUE, LLVMValueRef, Constant), sem_const_insert_element(LLVMConstInsertElement, vector_constant, element_value_constant, index_constant))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMConstInt, LOCKED, VALUE, (REF, int_ty, TYPE, LLVMTypeRef, IntegerTypeKind), (SCAL, value, unsigned long long), (SCAL, sign_extend, bool))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMConstIntGetSExtValue, LOCKED, long long, (REF, constant_val, VALUE, LLVMValueRef, ConstantInt), sem_const_int_sext_fits(LLVMConstIntGetSExtValue, constant_val))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMConstIntGetZExtValue, LOCKED, unsigned long long, (REF, constant_val, VALUE, LLVMValueRef, ConstantInt), sem_const_int_zext_fits(LLVMConstIntGetZExtValue, constant_val))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMConstIntOfString, LOCKED, VALUE, (REF, int_ty, TYPE, LLVMTypeRef, IntegerTypeKind), (CSTR, text), (SCAL, radix, unsigned), sem_apint_string(LLVMConstIntOfString, int_ty, text, radix))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMConstIntOfStringAndSize, LOCKED, VALUE, (REF, int_ty, TYPE, LLVMTypeRef, IntegerTypeKind), (OPTSTR, text, s_len), (SCAL, s_len, unsigned), (SCAL, radix, unsigned), sem_apint_string_len(LLVMConstIntOfStringAndSize, int_ty, text, s_len, radix))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstIntToPtr, LOCKED, VALUE, (REF, constant_val, VALUE, LLVMValueRef, Constant), (REF, to_type, TYPE, LLVMTypeRef), sem_const_int_to_ptr(LLVMConstIntToPtr, constant_val, to_type))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstMul, LOCKED, VALUE, (REF, lhs_constant, VALUE, LLVMValueRef, Constant), (REF, rhs_constant, VALUE, LLVMValueRef, Constant), sem_int_binop(LLVMConstMul, lhs_constant, rhs_constant))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstNSWAdd, LOCKED, VALUE, (REF, lhs_constant, VALUE, LLVMValueRef, Constant), (REF, rhs_constant, VALUE, LLVMValueRef, Constant), sem_int_binop(LLVMConstNSWAdd, lhs_constant, rhs_constant))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstNSWMul, LOCKED, VALUE, (REF, lhs_constant, VALUE, LLVMValueRef, Constant), (REF, rhs_constant, VALUE, LLVMValueRef, Constant), sem_int_binop(LLVMConstNSWMul, lhs_constant, rhs_constant))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMConstNSWNeg, LOCKED, VALUE, (REF, constant_val, VALUE, LLVMValueRef, Constant), sem_const_unary_int(LLVMConstNSWNeg, constant_val))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstNSWSub, LOCKED, VALUE, (REF, lhs_constant, VALUE, LLVMValueRef, Constant), (REF, rhs_constant, VALUE, LLVMValueRef, Constant), sem_int_binop(LLVMConstNSWSub, lhs_constant, rhs_constant))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstNUWAdd, LOCKED, VALUE, (REF, lhs_constant, VALUE, LLVMValueRef, Constant), (REF, rhs_constant, VALUE, LLVMValueRef, Constant), sem_int_binop(LLVMConstNUWAdd, lhs_constant, rhs_constant))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstNUWMul, LOCKED, VALUE, (REF, lhs_constant, VALUE, LLVMValueRef, Constant), (REF, rhs_constant, VALUE, LLVMValueRef, Constant), sem_int_binop(LLVMConstNUWMul, lhs_constant, rhs_constant))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstNUWSub, LOCKED, VALUE, (REF, lhs_constant, VALUE, LLVMValueRef, Constant), (REF, rhs_constant, VALUE, LLVMValueRef, Constant), sem_int_binop(LLVMConstNUWSub, lhs_constant, rhs_constant))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMConstNamedStruct, LOCKED, VALUE, (REF, struct_ty, TYPE, LLVMTypeRef, StructTypeKind), (ARR, constant_vals, count, VALUE, LLVMValueRef, Constant), (SCAL, count, unsigned), sem_const_named_struct(LLVMConstNamedStruct, struct_ty, constant_vals, count))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMConstNeg, LOCKED, VALUE, (REF, constant_val, VALUE, LLVMValueRef, Constant), sem_const_unary_int(LLVMConstNeg, constant_val))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMConstNot, LOCKED, VALUE, (REF, constant_val, VALUE, LLVMValueRef, Constant), sem_const_unary_int(LLVMConstNot, constant_val))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMConstNull, LOCKED, VALUE, (REF, type, TYPE, LLVMTypeRef), sem_const_null(LLVMConstNull, type))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstPointerCast, LOCKED, VALUE, (REF, constant_val, VALUE, LLVMValueRef, Constant), (REF, to_type, TYPE, LLVMTypeRef), sem_const_pointer_cast(LLVMConstPointerCast, constant_val, to_type))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMConstPointerNull, LOCKED, VALUE, (REF, type, TYPE, LLVMTypeRef, PointerTypeKind))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstPtrToInt, LOCKED, VALUE, (REF, constant_val, VALUE, LLVMValueRef, Constant), (REF, to_type, TYPE, LLVMTypeRef), sem_const_ptr_to_int(LLVMConstPtrToInt, constant_val, to_type))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstReal, LOCKED, VALUE, (REF, real_ty, TYPE, LLVMTypeRef), (SCAL, real_value, double), sem_fp_ty(LLVMConstReal, real_ty))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstRealOfString, LOCKED, VALUE, (REF, real_ty, TYPE, LLVMTypeRef), (CSTR, text), sem_fp_ty(LLVMConstRealOfString, real_ty) sem_apfloat_string(LLVMConstRealOfString, real_ty, text))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMConstRealOfStringAndSize, LOCKED, VALUE, (REF, real_ty, TYPE, LLVMTypeRef), (OPTSTR, text, s_len), (SCAL, s_len, unsigned), sem_fp_ty(LLVMConstRealOfStringAndSize, real_ty) sem_apfloat_string_len(LLVMConstRealOfStringAndSize, real_ty, text, s_len))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMConstShuffleVector, LOCKED, VALUE, (REF, vector_a_constant, VALUE, LLVMValueRef, Constant), (REF, vector_b_constant, VALUE, LLVMValueRef, Constant), (REF, mask_constant, VALUE, LLVMValueRef, Constant), sem_const_shuffle_vector(LLVMConstShuffleVector, vector_a_constant, vector_b_constant, mask_constant))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMConstStringInContext, LOCKED, VALUE, (CTX, context), (OPTSTR, str, length), (SCAL, length, unsigned), (SCAL, dont_null_terminate, bool))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMConstStringInContext2, LOCKED, VALUE, (CTX, context), (OPTSTR, str, length), (SCAL, length, size_t), (SCAL, dont_null_terminate, bool))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMConstStructInContext, LOCKED, VALUE, (CTX, context), (ARR, constant_vals, count, VALUE, LLVMValueRef, Constant), (SCAL, count, unsigned), (SCAL, packed, bool))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstSub, LOCKED, VALUE, (REF, lhs_constant, VALUE, LLVMValueRef, Constant), (REF, rhs_constant, VALUE, LLVMValueRef, Constant), sem_int_binop(LLVMConstSub, lhs_constant, rhs_constant))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstTrunc, LOCKED, VALUE, (REF, constant_val, VALUE, LLVMValueRef, Constant), (REF, to_type, TYPE, LLVMTypeRef), sem_const_trunc(LLVMConstTrunc, constant_val, to_type))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstTruncOrBitCast, LOCKED, VALUE, (REF, constant_val, VALUE, LLVMValueRef, Constant), (REF, to_type, TYPE, LLVMTypeRef), sem_const_trunc_or_bit_cast(LLVMConstTruncOrBitCast, constant_val, to_type))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstVector, LOCK_IF_CONTEXT, VALUE, (ARR, scalar_constant_vals, size, VALUE, LLVMValueRef, Constant), (SCAL, size, unsigned), sem_nonempty(LLVMConstVector, size))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMConstXor, LOCKED, VALUE, (REF, lhs_constant, VALUE, LLVMValueRef, Constant), (REF, rhs_constant, VALUE, LLVMValueRef, Constant), sem_int_binop(LLVMConstXor, lhs_constant, rhs_constant))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMConstantPtrAuth, LOCKED, VALUE, (REF, ptr, VALUE, LLVMValueRef, Constant), (REF, key, VALUE, LLVMValueRef, ConstantInt), (REF, disc, VALUE, LLVMValueRef, ConstantInt), (REF, addr_disc, VALUE, LLVMValueRef, Constant), sem_const_ptr_auth(LLVMConstantPtrAuth, ptr, key, disc, addr_disc))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMContextSetDiscardValueNames, LOCKED, (CTX, context), (SCAL, discard, bool))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMContextShouldDiscardValueNames, LOCKED, bool, (CTX, context))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMCountBasicBlocks, LOCKED, unsigned, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMCountIncoming, LOCKED, unsigned, (REF, phi_node, VALUE, LLVMValueRef, PHINode))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMCountParamTypes, LOCKED, unsigned, (REF, function_ty, TYPE, LLVMTypeRef, FunctionTypeKind))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMCountParams, LOCKED, unsigned, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMCountStructElementTypes, LOCKED, unsigned, (REF, struct_ty, TYPE, LLVMTypeRef, StructTypeKind))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMCreateBasicBlockInContext, LOCKED, BASIC_BLOCK, (CTX, context), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMCreateBuilderInContext, LOCKED, BUILDER, (CTX, context))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMCreateEnumAttribute, LOCKED, ATTRIBUTE, (CTX, context), (SCAL, kind_id, unsigned), (SCAL, val, unsigned long long), sem_enum_attr(LLVMCreateEnumAttribute, kind_id, val))
FILC_LLVM_JIT_DEFINE_REF_5(LLVMCreateStringAttribute, LOCKED, ATTRIBUTE, (CTX, context), (OPTSTR, key, k_length), (SCAL, k_length, unsigned), (OPTSTR, value, v_length), (SCAL, v_length, unsigned))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMCreateTypeAttribute, LOCKED, ATTRIBUTE, (CTX, context), (SCAL, kind_id, unsigned), (REF, type_ref, TYPE, LLVMTypeRef), sem_type_attr_kind(LLVMCreateTypeAttribute, kind_id))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMDoubleTypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_VOID_1(LLVMDumpModule, LOCKED, (REF, module, MODULE, LLVMModuleRef))
FILC_LLVM_JIT_DEFINE_VOID_1(LLVMDumpType, LOCKED, (REF, val, TYPE, LLVMTypeRef))
FILC_LLVM_JIT_DEFINE_VOID_1(LLVMDumpValue, LOCKED, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMFP128TypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMFloatTypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMFunctionType, LOCKED, TYPE, (REF, return_type, TYPE, LLVMTypeRef), (ARR, param_types, param_count, TYPE, LLVMTypeRef), (SCAL, param_count, unsigned), (SCAL, is_var_arg, bool), sem_fn_ret_ty(LLVMFunctionType, return_type) sem_fn_params(LLVMFunctionType, param_types, param_count))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGEPGetNoWrapFlags, LOCKED, unsigned, (REF, gep, VALUE, LLVMValueRef, GEPOperator))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMGEPSetNoWrapFlags, LOCKED, (REF, gep, VALUE, LLVMValueRef, GetElementPtrInst), (SCAL, no_wrap_flags, unsigned))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetAggregateElement, LOCKED, VALUE, (REF, value, VALUE, LLVMValueRef, Constant), (SCAL, idx, unsigned), sem_aggregate_element(LLVMGetAggregateElement, value))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetAlignment, LOCKED, unsigned, (REF, value, VALUE, LLVMValueRef, AlignmentOwner))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetAllocatedType, LOCKED, TYPE, (REF, alloca, VALUE, LLVMValueRef, AllocaInst))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetArgOperand, LOCKED, VALUE, (REF, funclet, VALUE, LLVMValueRef, FuncletPadInst), (SCAL, index, unsigned), sem_arg_operand_index(LLVMGetArgOperand, funclet, index))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetArrayLength, LOCKED, unsigned, (REF, array_ty, TYPE, LLVMTypeRef, ArrayTypeKind))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetArrayLength2, LOCKED, unsigned long long, (REF, array_ty, TYPE, LLVMTypeRef, ArrayTypeKind))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetAtomicRMWBinOp, LOCKED, unsigned, (REF, atomic_rmw_inst, VALUE, LLVMValueRef, AtomicRMWInst))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetAtomicSyncScopeID, LOCKED, unsigned, (REF, atomic_inst, VALUE, LLVMValueRef, Instruction), sem_atomic_inst(LLVMGetAtomicSyncScopeID, atomic_inst))
FILC_LLVM_JIT_DEFINE_SCALAR_2(LLVMGetAttributeCountAtIndex, LOCKED, unsigned, (REF, value, VALUE, LLVMValueRef, Function), (SCAL, idx, unsigned))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetBasicBlockParent, LOCKED, VALUE, (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetBasicBlockTerminator, LOCKED, VALUE, (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetBlockAddressBasicBlock, LOCKED, BASIC_BLOCK, (REF, block_addr, VALUE, LLVMValueRef, BlockAddress))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetBlockAddressFunction, LOCKED, VALUE, (REF, block_addr, VALUE, LLVMValueRef, BlockAddress))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetBuilderContext, LOCKED, CONTEXT, (REF, builder, BUILDER, LLVMBuilderRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetCallBrDefaultDest, LOCKED, BASIC_BLOCK, (REF, call_br, VALUE, LLVMValueRef, CallBrInst))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetCallBrIndirectDest, LOCKED, BASIC_BLOCK, (REF, call_br, VALUE, LLVMValueRef, CallBrInst), (SCAL, idx, unsigned), sem_callbr_indirect_dest_index(LLVMGetCallBrIndirectDest, call_br, idx))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetCallBrNumIndirectDests, LOCKED, unsigned, (REF, call_br, VALUE, LLVMValueRef, CallBrInst))
FILC_LLVM_JIT_DEFINE_SCALAR_2(LLVMGetCallSiteAttributeCount, LOCKED, unsigned, (REF, value, VALUE, LLVMValueRef, CallBase), (SCAL, idx, unsigned))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMGetCallSiteEnumAttribute, LOCKED, ATTRIBUTE, (REF, value, VALUE, LLVMValueRef, CallBase), (SCAL, idx, unsigned), (SCAL, kind_id, unsigned))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMGetCallSiteStringAttribute, LOCKED, ATTRIBUTE, (REF, value, VALUE, LLVMValueRef, CallBase), (SCAL, idx, unsigned), (OPTSTR, key, k_len), (SCAL, k_len, unsigned))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetCalledFunctionType, LOCKED, TYPE, (REF, value, VALUE, LLVMValueRef, CallBase))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetCalledValue, LOCKED, VALUE, (REF, instr, VALUE, LLVMValueRef, CallBase))
FILC_LLVM_JIT_DEFINE_SCALAR_4(LLVMGetCastOpcode, LOCKED, unsigned, (REF, src, VALUE, LLVMValueRef), (SCAL, src_is_signed, bool), (REF, dest_ty, TYPE, LLVMTypeRef), (SCAL, dest_is_signed, bool), sem_cast_opcode(LLVMGetCastOpcode, src, dest_ty))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetClause, LOCKED, VALUE, (REF, landing_pad, VALUE, LLVMValueRef, LandingPadInst), (SCAL, idx, unsigned), sem_clause_index(LLVMGetClause, landing_pad, idx))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetCmpXchgFailureOrdering, LOCKED, unsigned, (REF, cmp_xchg_inst, VALUE, LLVMValueRef, AtomicCmpXchgInst))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetCmpXchgSuccessOrdering, LOCKED, unsigned, (REF, cmp_xchg_inst, VALUE, LLVMValueRef, AtomicCmpXchgInst))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetCondition, LOCKED, VALUE, (REF, branch, VALUE, LLVMValueRef, BranchInst))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetConstOpcode, LOCKED, unsigned, (REF, constant_val, VALUE, LLVMValueRef, ConstantExpr))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetConstantPtrAuthAddrDiscriminator, LOCKED, VALUE, (REF, ptr_auth, VALUE, LLVMValueRef, ConstantPtrAuth))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetConstantPtrAuthDiscriminator, LOCKED, VALUE, (REF, ptr_auth, VALUE, LLVMValueRef, ConstantPtrAuth))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetConstantPtrAuthKey, LOCKED, VALUE, (REF, ptr_auth, VALUE, LLVMValueRef, ConstantPtrAuth))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetConstantPtrAuthPointer, LOCKED, VALUE, (REF, ptr_auth, VALUE, LLVMValueRef, ConstantPtrAuth))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetCurrentDebugLocation, LOCKED, VALUE, (REF, builder, BUILDER, LLVMBuilderRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetCurrentDebugLocation2, LOCKED, METADATA, (REF, builder, BUILDER, LLVMBuilderRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetDLLStorageClass, LOCKED, unsigned, (REF, global, VALUE, LLVMValueRef, GlobalValue))
/* LLVMGetDebugLocColumn is graceful in the C API (no subclass assert) -- no
   check needed. */
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetDebugLocColumn, LOCKED, unsigned, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetDebugLocLine, LOCKED, unsigned, (REF, val, VALUE, LLVMValueRef, DebugLocOwner))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetElementType, LOCKED, TYPE, (REF, type, TYPE, LLVMTypeRef, SequenceTypeKind))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetEntryBasicBlock, LOCKED, BASIC_BLOCK, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMGetEnumAttributeAtIndex, LOCKED, ATTRIBUTE, (REF, value, VALUE, LLVMValueRef, Function), (SCAL, idx, unsigned), (SCAL, kind_id, unsigned))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetEnumAttributeKind, LOCKED, unsigned, (REF, attribute, ATTRIBUTE, LLVMAttributeRef, NotStringAttribute))
FILC_LLVM_JIT_DEFINE_SCALAR_2(LLVMGetEnumAttributeKindForName, NO_LOCK, unsigned, (OPTSTR, name, s_len), (SCAL, s_len, size_t))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetEnumAttributeValue, LOCKED, unsigned long long, (REF, attribute, ATTRIBUTE, LLVMAttributeRef, EnumAttribute))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetExact, LOCKED, bool, (REF, div_or_shr_inst, VALUE, LLVMValueRef, Instruction))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetFCmpPredicate, LOCKED, unsigned, (REF, inst, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetFastMathFlags, LOCKED, unsigned, (REF, fp_math_inst, VALUE, LLVMValueRef, Instruction), sem_fpmath(LLVMGetFastMathFlags, fp_math_inst))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetFirstBasicBlock, LOCKED, BASIC_BLOCK, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetFirstDbgRecord, LOCKED, DBG_RECORD, (REF, inst, VALUE, LLVMValueRef, Instruction))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetFirstFunction, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetFirstGlobal, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetFirstGlobalAlias, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetFirstGlobalIFunc, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetFirstInstruction, LOCKED, VALUE, (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetFirstNamedMetadata, LOCKED, NAMED_METADATA, (REF, module, MODULE, LLVMModuleRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetFirstParam, LOCKED, VALUE, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetFirstUse, LOCKED, USE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetFunctionCallConv, LOCKED, unsigned, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetGEPSourceElementType, LOCKED, TYPE, (REF, gep, VALUE, LLVMValueRef, GEPOperator))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetGlobalIFuncResolver, LOCKED, VALUE, (REF, i_func, VALUE, LLVMValueRef, GlobalIFunc))
/* Fil-C: LLVMGetGlobalParent cannot go through FILC_LLVM_JIT_DEFINE_REF_1.
 * The generic wrapper would wrap the returned module handle in a FRESH GC
 * object -- a second wrapper for one LLVM module.  A fresh wrapper either
 * disposes a module that is still in use (it owns the handle; the first
 * of the two wrappers to die kills the module out from under the other) or,
 * if taught not to dispose, dangles once the owning wrapper dies first.
 * The wrapper we hand out for a global already points at the module object
 * that is the parent (filc_llvm_jit_object.module, set at wrap time from the
 * module the global was created/fetched through).  So assert that the LLVM
 * parent really is that module's handle and return the very same wrapper:
 * identity is preserved for callers that compare module refs, the GC object
 * is shared rather than duplicated, and disposal stays one-to-one with the
 * module. */
filc_ptr filc_native_LLVMGetGlobalParent(filc_thread* my_thread, filc_ptr global)
{
    ensure_api(my_thread);
    FILC_LLVM_JIT_DECLS(LOCKED)
    FILC_LLVM_JIT_UNWRAP(LLVMGetGlobalParent,
                         (REF, global, VALUE, LLVMValueRef, GlobalValue))
    FILC_LLVM_JIT_LOCK(LOCKED)
    FILC_LLVM_JIT_VALIDATE(LLVMGetGlobalParent,
                           (REF, global, VALUE, LLVMValueRef, GlobalValue))
    void* parent_raw =
        api->LLVMGetGlobalParent((LLVMValueRef)global_obj->handle);
    FILC_CHECK(global_obj->module, NULL,
               "Fil-C LLVM JIT API: LLVMGetGlobalParent on a global whose "
               "wrapper has no affiliated module object.");
    /* The object the value points at IS the module that is the parent. */
    FILC_CHECK(parent_raw == global_obj->module->base.handle, NULL,
               "Fil-C LLVM JIT API: a global wrapper's affiliated module "
               "object is not its LLVM global parent.");
    filc_ptr result = filc_ptr_create_with_special_object(
        my_thread, filc_object_for_special_payload(global_obj->module));
    FILC_LLVM_JIT_UNLOCK(LOCKED)
    return result;
}

FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetICmpPredicate, LOCKED, unsigned, (REF, inst, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetIncomingBlock, LOCKED, BASIC_BLOCK, (REF, phi_node, VALUE, LLVMValueRef, PHINode), (SCAL, index, unsigned), sem_incoming_index(LLVMGetIncomingBlock, phi_node, index))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetIncomingValue, LOCKED, VALUE, (REF, phi_node, VALUE, LLVMValueRef, PHINode), (SCAL, index, unsigned), sem_incoming_index(LLVMGetIncomingValue, phi_node, index))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetInitializer, LOCKED, VALUE, (REF, global_var, VALUE, LLVMValueRef, GlobalVariable))
FILC_LLVM_JIT_DEFINE_REF_9(LLVMGetInlineAsm, LOCKED, VALUE, (REF, type, TYPE, LLVMTypeRef, FunctionTypeKind), (OPTSTR, asm_string, asm_string_size), (SCAL, asm_string_size, size_t), (OPTSTR, constraints, constraints_size), (SCAL, constraints_size, size_t), (SCAL, has_side_effects, bool), (SCAL, is_align_stack, bool), (SCAL, dialect, unsigned), (SCAL, can_throw, bool), sem_inline_asm(LLVMGetInlineAsm, type, constraints, constraints_size, dialect))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetInlineAsmCanUnwind, LOCKED, bool, (REF, inline_asm_val, VALUE, LLVMValueRef, InlineAsm))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetInlineAsmDialect, LOCKED, unsigned, (REF, inline_asm_val, VALUE, LLVMValueRef, InlineAsm))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetInlineAsmFunctionType, LOCKED, TYPE, (REF, inline_asm_val, VALUE, LLVMValueRef, InlineAsm))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetInlineAsmHasSideEffects, LOCKED, bool, (REF, inline_asm_val, VALUE, LLVMValueRef, InlineAsm))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetInlineAsmNeedsAlignedStack, LOCKED, bool, (REF, inline_asm_val, VALUE, LLVMValueRef, InlineAsm))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetInsertBlock, LOCKED, BASIC_BLOCK, (REF, builder, BUILDER, LLVMBuilderRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetInstructionCallConv, LOCKED, unsigned, (REF, instr, VALUE, LLVMValueRef, CallBase))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetInstructionOpcode, LOCKED, unsigned, (REF, inst, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetInstructionParent, LOCKED, BASIC_BLOCK, (REF, inst, VALUE, LLVMValueRef, Instruction))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetIntTypeWidth, LOCKED, unsigned, (REF, integer_ty, TYPE, LLVMTypeRef, IntegerTypeKind))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMGetIntrinsicDeclaration, LOCKED, VALUE, (REF, mod, MODULE, LLVMModuleRef), (SCAL, intrinsic_id, unsigned), (ARR, param_types, param_count, TYPE, LLVMTypeRef), (SCAL, param_count, size_t), sem_intrinsic_id(LLVMGetIntrinsicDeclaration, intrinsic_id) sem_intrinsic_tys_exact(LLVMGetIntrinsicDeclaration, intrinsic_id, param_count) sem_intrinsic_tys_ok(LLVMGetIntrinsicDeclaration, intrinsic_id, param_types, param_count) sem_intrinsic_tys_mangle_ok(LLVMGetIntrinsicDeclaration, param_types, param_count))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetIntrinsicID, LOCKED, unsigned, (REF, function, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetIsDisjoint, LOCKED, bool, (REF, inst, VALUE, LLVMValueRef, PossiblyDisjointInst))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetLastBasicBlock, LOCKED, BASIC_BLOCK, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetLastDbgRecord, LOCKED, DBG_RECORD, (REF, inst, VALUE, LLVMValueRef, Instruction))
FILC_LLVM_JIT_DEFINE_SCALAR_0(LLVMGetLastEnumAttributeKind, NO_LOCK, unsigned)
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetLastFunction, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetLastGlobal, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetLastGlobalAlias, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetLastGlobalIFunc, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetLastInstruction, LOCKED, VALUE, (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetLastNamedMetadata, LOCKED, NAMED_METADATA, (REF, module, MODULE, LLVMModuleRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetLastParam, LOCKED, VALUE, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetLinkage, LOCKED, unsigned, (REF, global, VALUE, LLVMValueRef, GlobalValue))
FILC_LLVM_JIT_DEFINE_SCALAR_3(LLVMGetMDKindIDInContext, LOCKED, unsigned, (CTX, context), (OPTSTR, name, s_len), (SCAL, s_len, unsigned))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetMDNodeNumOperands, LOCKED, unsigned, (REF, value, VALUE, LLVMValueRef, MDNodeOrValueAsMetadataValue))
FILC_LLVM_JIT_DEFINE_SCALAR_2(LLVMGetMaskValue, LOCKED, int, (REF, shuffle_vector_inst, VALUE, LLVMValueRef, ShuffleVectorInst), (SCAL, elt, unsigned), sem_mask_index(LLVMGetMaskValue, shuffle_vector_inst, elt))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetMetadata, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef, Instruction), (SCAL, kind_id, unsigned))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetModuleContext, LOCKED, CONTEXT, (REF, module, MODULE, LLVMModuleRef))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMGetModuleFlag, LOCKED, METADATA, (REF, module, MODULE, LLVMModuleRef), (OPTSTR, key, key_len), (SCAL, key_len, size_t))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetNNeg, LOCKED, bool, (REF, non_neg_inst, VALUE, LLVMValueRef, Instruction))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetNSW, LOCKED, bool, (REF, arith_inst, VALUE, LLVMValueRef, Instruction))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetNUW, LOCKED, bool, (REF, arith_inst, VALUE, LLVMValueRef, Instruction))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetNamedFunction, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMGetNamedFunctionWithLength, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef), (OPTSTR, name, length), (SCAL, length, size_t))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetNamedGlobal, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMGetNamedGlobalAlias, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef), (OPTSTR, name, name_len), (SCAL, name_len, size_t))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMGetNamedGlobalIFunc, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef), (OPTSTR, name, name_len), (SCAL, name_len, size_t))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMGetNamedGlobalWithLength, LOCKED, VALUE, (REF, module, MODULE, LLVMModuleRef), (OPTSTR, name, length), (SCAL, length, size_t))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMGetNamedMetadata, LOCKED, NAMED_METADATA, (REF, module, MODULE, LLVMModuleRef), (OPTSTR, name, name_len), (SCAL, name_len, size_t))
FILC_LLVM_JIT_DEFINE_SCALAR_2(LLVMGetNamedMetadataNumOperands, LOCKED, unsigned, (REF, module, MODULE, LLVMModuleRef), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetNextBasicBlock, LOCKED, BASIC_BLOCK, (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetNextDbgRecord, LOCKED, DBG_RECORD, (REF, dbg_record, DBG_RECORD, LLVMDbgRecordRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetNextFunction, LOCKED, VALUE, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetNextGlobal, LOCKED, VALUE, (REF, global_var, VALUE, LLVMValueRef, GlobalVariable))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetNextGlobalAlias, LOCKED, VALUE, (REF, alias, VALUE, LLVMValueRef, GlobalAlias))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetNextGlobalIFunc, LOCKED, VALUE, (REF, i_func, VALUE, LLVMValueRef, GlobalIFunc))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetNextInstruction, LOCKED, VALUE, (REF, inst, VALUE, LLVMValueRef, Instruction))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetNextNamedMetadata, LOCKED, NAMED_METADATA, (REF, named_md_node, NAMED_METADATA, LLVMNamedMDNodeRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetNextParam, LOCKED, VALUE, (REF, arg, VALUE, LLVMValueRef, Argument))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetNextUse, LOCKED, USE, (REF, use, USE, LLVMUseRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetNormalDest, LOCKED, BASIC_BLOCK, (REF, invoke_inst, VALUE, LLVMValueRef, InvokeInst))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetNumArgOperands, LOCKED, unsigned, (REF, instr, VALUE, LLVMValueRef, CallBaseOrFuncletPadInst))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetNumClauses, LOCKED, unsigned, (REF, landing_pad, VALUE, LLVMValueRef, LandingPadInst))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetNumContainedTypes, LOCKED, unsigned, (REF, type, TYPE, LLVMTypeRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetNumHandlers, LOCKED, unsigned, (REF, catch_switch, VALUE, LLVMValueRef, CatchSwitchInst))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetNumIndices, LOCKED, unsigned, (REF, inst, VALUE, LLVMValueRef, NumIndicesOwner))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetNumMaskElements, LOCKED, unsigned, (REF, shuffle_vector_inst, VALUE, LLVMValueRef, ShuffleVectorInst))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetNumOperands, LOCKED, int, (REF, val, VALUE, LLVMValueRef, UserOrMDValue))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetNumSuccessors, LOCKED, unsigned, (REF, term, VALUE, LLVMValueRef, Instruction))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetOperand, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef, UserOrMDValue), (SCAL, index, unsigned), sem_operand_index(LLVMGetOperand, val, index))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetOperandUse, LOCKED, USE, (REF, val, VALUE, LLVMValueRef, User), (SCAL, index, unsigned), sem_operand_index(LLVMGetOperandUse, val, index))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMGetOrInsertNamedMetadata, LOCKED, NAMED_METADATA, (REF, module, MODULE, LLVMModuleRef), (OPTSTR, name, name_len), (SCAL, name_len, size_t))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetOrdering, LOCKED, unsigned, (REF, memory_access_inst, VALUE, LLVMValueRef, OrderingOwner))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetParam, LOCKED, VALUE, (REF, function, VALUE, LLVMValueRef, Function), (SCAL, index, unsigned), sem_param_index(LLVMGetParam, function, index))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetParamParent, LOCKED, VALUE, (REF, inst, VALUE, LLVMValueRef, Argument))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetParentCatchSwitch, LOCKED, VALUE, (REF, catch_pad, VALUE, LLVMValueRef, CatchPadInst))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetPersonalityFn, LOCKED, VALUE, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetPointerAddressSpace, LOCKED, unsigned, (REF, pointer_ty, TYPE, LLVMTypeRef, PointerTypeKind))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetPoison, LOCKED, VALUE, (REF, type, TYPE, LLVMTypeRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetPrefixData, LOCKED, VALUE, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetPreviousBasicBlock, LOCKED, BASIC_BLOCK, (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetPreviousDbgRecord, LOCKED, DBG_RECORD, (REF, dbg_record, DBG_RECORD, LLVMDbgRecordRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetPreviousFunction, LOCKED, VALUE, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetPreviousGlobal, LOCKED, VALUE, (REF, global_var, VALUE, LLVMValueRef, GlobalVariable))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetPreviousGlobalAlias, LOCKED, VALUE, (REF, alias, VALUE, LLVMValueRef, GlobalAlias))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetPreviousGlobalIFunc, LOCKED, VALUE, (REF, i_func, VALUE, LLVMValueRef, GlobalIFunc))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetPreviousInstruction, LOCKED, VALUE, (REF, inst, VALUE, LLVMValueRef, Instruction))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetPreviousNamedMetadata, LOCKED, NAMED_METADATA, (REF, named_md_node, NAMED_METADATA, LLVMNamedMDNodeRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetPreviousParam, LOCKED, VALUE, (REF, arg, VALUE, LLVMValueRef, Argument))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetPrologueData, LOCKED, VALUE, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetReturnType, LOCKED, TYPE, (REF, function_ty, TYPE, LLVMTypeRef, FunctionTypeKind))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMGetStringAttributeAtIndex, LOCKED, ATTRIBUTE, (REF, value, VALUE, LLVMValueRef, Function), (SCAL, idx, unsigned), (OPTSTR, key, k_len), (SCAL, k_len, unsigned))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetSuccessor, LOCKED, BASIC_BLOCK, (REF, term, VALUE, LLVMValueRef, Instruction), (SCAL, index, unsigned), sem_successor_index(LLVMGetSuccessor, term, index))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetSwitchDefaultDest, LOCKED, BASIC_BLOCK, (REF, switch_instr, VALUE, LLVMValueRef, SwitchInst))
FILC_LLVM_JIT_DEFINE_SCALAR_3(LLVMGetSyncScopeID, LOCKED, unsigned, (CTX, context), (OPTSTR, name, s_len), (SCAL, s_len, size_t))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetTailCallKind, LOCKED, unsigned, (REF, call_inst, VALUE, LLVMValueRef, CallInst))
FILC_LLVM_JIT_DEFINE_SCALAR_2(LLVMGetTargetExtTypeIntParam, LOCKED, unsigned, (REF, target_ext_ty, TYPE, LLVMTypeRef, TargetExtTypeKind), (SCAL, idx, unsigned), sem_target_ext_int_param_index(LLVMGetTargetExtTypeIntParam, target_ext_ty, idx))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetTargetExtTypeNumIntParams, LOCKED, unsigned, (REF, target_ext_ty, TYPE, LLVMTypeRef, TargetExtTypeKind))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetTargetExtTypeNumTypeParams, LOCKED, unsigned, (REF, target_ext_ty, TYPE, LLVMTypeRef, TargetExtTypeKind))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetTargetExtTypeTypeParam, LOCKED, TYPE, (REF, target_ext_ty, TYPE, LLVMTypeRef, TargetExtTypeKind), (SCAL, idx, unsigned), sem_target_ext_type_param_index(LLVMGetTargetExtTypeTypeParam, target_ext_ty, idx))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetThreadLocalMode, LOCKED, unsigned, (REF, global_var, VALUE, LLVMValueRef, GlobalVariable))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetTypeAttributeValue, LOCKED, TYPE, (REF, attribute, ATTRIBUTE, LLVMAttributeRef, TypeAttribute))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetTypeByName, LOCKED, TYPE, (REF, module, MODULE, LLVMModuleRef), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMGetTypeByName2, LOCKED, TYPE, (CTX, context), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetTypeContext, LOCKED, CONTEXT, (REF, type, TYPE, LLVMTypeRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetTypeKind, LOCKED, unsigned, (REF, type, TYPE, LLVMTypeRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetUndef, LOCKED, VALUE, (REF, type, TYPE, LLVMTypeRef))
FILC_LLVM_JIT_DEFINE_SCALAR_0(LLVMGetUndefMaskElem, NO_LOCK, int)
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetUnnamedAddress, LOCKED, unsigned, (REF, global, VALUE, LLVMValueRef, GlobalValue))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetUnwindDest, LOCKED, BASIC_BLOCK, (REF, invoke_inst, VALUE, LLVMValueRef, UnwindOwner))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetUsedValue, LOCKED, VALUE, (REF, use, USE, LLVMUseRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetUser, LOCKED, VALUE, (REF, use, USE, LLVMUseRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGetValueContext, LOCKED, CONTEXT, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetValueKind, LOCKED, unsigned, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetVectorSize, LOCKED, unsigned, (REF, vector_ty, TYPE, LLVMTypeRef, VectorTypeKind))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetVisibility, LOCKED, unsigned, (REF, global, VALUE, LLVMValueRef, GlobalValue))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetVolatile, LOCKED, bool, (REF, memory_access_inst, VALUE, LLVMValueRef, VolatileOwner))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMGetWeak, LOCKED, bool, (REF, cmp_xchg_inst, VALUE, LLVMValueRef, AtomicCmpXchgInst))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMGlobalGetValueType, LOCKED, TYPE, (REF, global, VALUE, LLVMValueRef, GlobalValue))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMGlobalSetMetadata, LOCKED, (REF, global, VALUE, LLVMValueRef, GlobalObject), (SCAL, kind, unsigned), (REF, metadata, METADATA, LLVMMetadataRef, MDNodeMetadata))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMHalfTypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMHasMetadata, LOCKED, int, (REF, val, VALUE, LLVMValueRef, Instruction))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMHasPersonalityFn, LOCKED, bool, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMHasPrefixData, LOCKED, bool, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMHasPrologueData, LOCKED, bool, (REF, function, VALUE, LLVMValueRef, Function))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMHasUnnamedAddr, LOCKED, bool, (REF, global, VALUE, LLVMValueRef, GlobalValue))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMInsertBasicBlockInContext, LOCKED, BASIC_BLOCK, (CTX, context), (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef), (CSTR, name))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMInsertExistingBasicBlockAfterInsertBlock, LOCKED, (REF, builder, BUILDER, LLVMBuilderRef), (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef), sem_insert_bb(LLVMInsertExistingBasicBlockAfterInsertBlock, builder, basic_block))
FILC_LLVM_JIT_DEFINE_VOID_2_ADOPT(LLVMInsertIntoBuilder, LOCKED, (REF, builder, BUILDER, LLVMBuilderRef), (REF, instr, VALUE, LLVMValueRef, Instruction), (ADOPT, builder), sem_detached(LLVMInsertIntoBuilder, builder, instr))
FILC_LLVM_JIT_DEFINE_VOID_3_ADOPT(LLVMInsertIntoBuilderWithName, LOCKED, (REF, builder, BUILDER, LLVMBuilderRef), (REF, instr, VALUE, LLVMValueRef, Instruction), (CSTR, name), (ADOPT, builder), sem_detached(LLVMInsertIntoBuilderWithName, builder, instr) sem_nameable_value(LLVMInsertIntoBuilderWithName, instr, name))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMInstructionClone, LOCKED, VALUE, (REF, inst, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMInt128TypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMInt16TypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMInt1TypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMInt32TypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMInt64TypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMInt8TypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMIntTypeInContext, LOCKED, TYPE, (CTX, context), (SCAL, num_bits, unsigned), sem_int_bits(LLVMIntTypeInContext, num_bits))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMIntrinsicGetType, LOCKED, TYPE, (CTX, ctx), (SCAL, intrinsic_id, unsigned), (ARR, param_types, param_count, TYPE, LLVMTypeRef), (SCAL, param_count, size_t), sem_intrinsic_id(LLVMIntrinsicGetType, intrinsic_id) sem_intrinsic_tys_overloaded(LLVMIntrinsicGetType, intrinsic_id, param_count) sem_intrinsic_tys_ok(LLVMIntrinsicGetType, intrinsic_id, param_types, param_count))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIntrinsicIsOverloaded, NO_LOCK, bool, (SCAL, intrinsic_id, unsigned), sem_intrinsic_id(LLVMIntrinsicIsOverloaded, intrinsic_id))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAAddrSpaceCastInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAAllocaInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAArgument, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAAtomicCmpXchgInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAAtomicRMWInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsABasicBlock, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsABinaryOperator, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsABitCastInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsABlockAddress, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsABranchInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsACallBrInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsACallInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsACastInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsACatchPadInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsACatchReturnInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsACatchSwitchInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsACleanupPadInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsACleanupReturnInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsACmpInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAConstant, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAConstantAggregateZero, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAConstantArray, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAConstantDataArray, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAConstantDataSequential, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAConstantDataVector, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAConstantExpr, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAConstantFP, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAConstantInt, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAConstantPointerNull, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAConstantPtrAuth, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAConstantStruct, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAConstantTokenNone, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAConstantVector, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsADbgDeclareInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsADbgInfoIntrinsic, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsADbgLabelInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsADbgVariableIntrinsic, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAExtractElementInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAExtractValueInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAFCmpInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAFPExtInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAFPToSIInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAFPToUIInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAFPTruncInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAFenceInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAFreezeInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAFuncletPadInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAFunction, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAGetElementPtrInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAGlobalAlias, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAGlobalIFunc, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAGlobalObject, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAGlobalValue, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAGlobalVariable, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAICmpInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAIndirectBrInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAInlineAsm, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAInsertElementInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAInsertValueInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAInstruction, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAIntToPtrInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAIntrinsicInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAInvokeInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsALandingPadInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsALoadInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAMemCpyInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAMemIntrinsic, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAMemMoveInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAMemSetInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAPHINode, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAPoisonValue, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAPtrToIntInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAResumeInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAReturnInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsASExtInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsASIToFPInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsASelectInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAShuffleVectorInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAStoreInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsASwitchInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsATruncInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAUIToFPInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAUnaryInstruction, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAUnaryOperator, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAUndefValue, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAUnreachableInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAUser, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAVAArgInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMIsAZExtInst, LOCKED, VALUE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsCleanup, LOCKED, bool, (REF, landing_pad, VALUE, LLVMValueRef, LandingPadInst))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsConditional, LOCKED, bool, (REF, branch, VALUE, LLVMValueRef, BranchInst))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsConstant, LOCKED, bool, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsConstantString, LOCKED, bool, (REF, string, VALUE, LLVMValueRef, ConstantDataSequential))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsDeclaration, LOCKED, bool, (REF, global, VALUE, LLVMValueRef, GlobalValue))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsEnumAttribute, LOCKED, bool, (REF, attribute, ATTRIBUTE, LLVMAttributeRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsExternallyInitialized, LOCKED, bool, (REF, global_var, VALUE, LLVMValueRef, GlobalVariable))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsFunctionVarArg, LOCKED, bool, (REF, function_ty, TYPE, LLVMTypeRef, FunctionTypeKind))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsGlobalConstant, LOCKED, bool, (REF, global_var, VALUE, LLVMValueRef, GlobalVariable))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsInBounds, LOCKED, bool, (REF, gep, VALUE, LLVMValueRef, GEPOperator))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsLiteralStruct, LOCKED, bool, (REF, struct_ty, TYPE, LLVMTypeRef, StructTypeKind))
FILC_LLVM_JIT_DEFINE_SCALAR_0(LLVMIsMultithreaded, NO_LOCK, bool)
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsNewDbgInfoFormat, LOCKED, bool, (REF, module, MODULE, LLVMModuleRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsNull, LOCKED, bool, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsOpaqueStruct, LOCKED, bool, (REF, struct_ty, TYPE, LLVMTypeRef, StructTypeKind))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsPackedStruct, LOCKED, bool, (REF, struct_ty, TYPE, LLVMTypeRef, StructTypeKind))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsPoison, LOCKED, bool, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsStringAttribute, LOCKED, bool, (REF, attribute, ATTRIBUTE, LLVMAttributeRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsTailCall, LOCKED, bool, (REF, call_inst, VALUE, LLVMValueRef, CallInst))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsThreadLocal, LOCKED, bool, (REF, global_var, VALUE, LLVMValueRef, GlobalVariable))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsTypeAttribute, LOCKED, bool, (REF, attribute, ATTRIBUTE, LLVMAttributeRef))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMIsUndef, LOCKED, bool, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMLabelTypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_SCALAR_2(LLVMLookupIntrinsicID, NO_LOCK, unsigned, (OPTSTR, name, name_len), (SCAL, name_len, size_t))
/* The MDNode constructors accept null elements: LLVMMDNodeInContext maps a
   null element to null metadata (Core.cpp: "if (!V) MD = nullptr;"), and
   LLVMMDNodeInContext2 forwards nulls to MDNode::get (textual IR !{null} is
   legal), so they carry tokens that skip the null-element check.
   LLVMMDNodeInContext's ElemMDNodeOperand token also mirrors the rest of
   its per-element logic (Core.cpp): a metadata-as-value wrapping
   function-local metadata is rejected regardless of the element count, and
   a raw value -- which Core.cpp treats as function-local metadata -- is
   legal only as the single element.  LLVMMDNodeInContext2 accepts
   arbitrary metadata, so it needs no element subclass check. */
FILC_LLVM_JIT_DEFINE_REF_3(LLVMMDNodeInContext, LOCKED, VALUE, (CTX, context), (ARR, vals, count, VALUE, LLVMValueRef, ElemMDNodeOperand), (SCAL, count, unsigned))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMMDNodeInContext2, LOCKED, METADATA, (CTX, context), (ARR, m_ds, count, METADATA, LLVMMetadataRef, ElemNullOK), (SCAL, count, size_t))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMMDStringInContext, LOCKED, VALUE, (CTX, context), (OPTSTR, str, s_len), (SCAL, s_len, unsigned))
FILC_LLVM_JIT_DEFINE_REF_3(LLVMMDStringInContext2, LOCKED, METADATA, (CTX, context), (OPTSTR, str, s_len), (SCAL, s_len, size_t))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMMetadataAsValue, LOCKED, VALUE, (CTX, context), (REF, metadata, METADATA, LLVMMetadataRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMMetadataTypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMModuleCreateWithNameInContext, LOCKED, MODULE, (CSTR, module_id), (CTX, context))
FILC_LLVM_JIT_DEFINE_SCALAR_2(LLVMModuleFlagEntriesGetFlagBehavior, LOCKED, unsigned, (REF, entries, MODULE_FLAG_ENTRIES, LLVMModuleFlagEntry*), (SCAL, index, unsigned), sem_entries_index(LLVMModuleFlagEntriesGetFlagBehavior, entries, index))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMModuleFlagEntriesGetMetadata, LOCKED, METADATA, (REF, entries, MODULE_FLAG_ENTRIES, LLVMModuleFlagEntry*), (SCAL, index, unsigned), sem_entries_index(LLVMModuleFlagEntriesGetMetadata, entries, index))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMMoveBasicBlockAfter, LOCKED, (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef), (REF, move_pos, BASIC_BLOCK, LLVMBasicBlockRef), sem_bb_bb(LLVMMoveBasicBlockAfter, basic_block, move_pos))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMMoveBasicBlockBefore, LOCKED, (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef), (REF, move_pos, BASIC_BLOCK, LLVMBasicBlockRef), sem_bb_bb(LLVMMoveBasicBlockBefore, basic_block, move_pos))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMPPCFP128TypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMPointerType, LOCKED, TYPE, (REF, element_type, TYPE, LLVMTypeRef), (SCAL, address_space, unsigned), sem_address_space(LLVMPointerType, address_space))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMPointerTypeInContext, LOCKED, TYPE, (CTX, context), (SCAL, address_space, unsigned), sem_address_space(LLVMPointerTypeInContext, address_space))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMPointerTypeIsOpaque, LOCKED, bool, (REF, type, TYPE, LLVMTypeRef))
FILC_LLVM_JIT_DEFINE_VOID_3_ADOPT(LLVMPositionBuilder, LOCKED, (REF, builder, BUILDER, LLVMBuilderRef), (REF, block, BASIC_BLOCK, LLVMBasicBlockRef), (REF, instr, VALUE, LLVMValueRef, Instruction), (ADOPT, builder), sem_position(LLVMPositionBuilder, block, instr))
FILC_LLVM_JIT_DEFINE_VOID_2_ADOPT(LLVMPositionBuilderAtEnd, LOCKED, (REF, builder, BUILDER, LLVMBuilderRef), (REF, block, BASIC_BLOCK, LLVMBasicBlockRef), (ADOPT, builder))
FILC_LLVM_JIT_DEFINE_VOID_2_ADOPT(LLVMPositionBuilderBefore, LOCKED, (REF, builder, BUILDER, LLVMBuilderRef), (REF, instr, VALUE, LLVMValueRef, Instruction), (ADOPT, builder))
FILC_LLVM_JIT_DEFINE_VOID_3_ADOPT(LLVMPositionBuilderBeforeDbgRecords, LOCKED, (REF, builder, BUILDER, LLVMBuilderRef), (REF, block, BASIC_BLOCK, LLVMBasicBlockRef), (REF, inst, VALUE, LLVMValueRef, Instruction), (ADOPT, builder), sem_position(LLVMPositionBuilderBeforeDbgRecords, block, inst))
FILC_LLVM_JIT_DEFINE_VOID_2_ADOPT(LLVMPositionBuilderBeforeInstrAndDbgRecords, LOCKED, (REF, builder, BUILDER, LLVMBuilderRef), (REF, instr, VALUE, LLVMValueRef, Instruction), (ADOPT, builder))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMRemoveCallSiteEnumAttribute, LOCKED, (REF, value, VALUE, LLVMValueRef, CallBase), (SCAL, idx, unsigned), (SCAL, kind_id, unsigned))
FILC_LLVM_JIT_DEFINE_VOID_4(LLVMRemoveCallSiteStringAttribute, LOCKED, (REF, value, VALUE, LLVMValueRef, CallBase), (SCAL, idx, unsigned), (OPTSTR, key, k_len), (SCAL, k_len, unsigned))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMRemoveEnumAttributeAtIndex, LOCKED, (REF, value, VALUE, LLVMValueRef, Function), (SCAL, idx, unsigned), (SCAL, kind_id, unsigned))
FILC_LLVM_JIT_DEFINE_VOID_4(LLVMRemoveStringAttributeAtIndex, LOCKED, (REF, value, VALUE, LLVMValueRef, Function), (SCAL, idx, unsigned), (OPTSTR, string, k_len), (SCAL, k_len, unsigned))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMReplaceAllUsesWith, LOCKED, (REF, old_val, VALUE, LLVMValueRef), (REF, new_val, VALUE, LLVMValueRef), sem_rauw(LLVMReplaceAllUsesWith, old_val, new_val))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMReplaceMDNodeOperandWith, LOCKED, (REF, value, VALUE, LLVMValueRef, MDNodeValue), (SCAL, index, unsigned), (REF, replacement, METADATA, LLVMMetadataRef), sem_md_operand_index(LLVMReplaceMDNodeOperandWith, value, index))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMScalableVectorType, LOCKED, TYPE, (REF, element_type, TYPE, LLVMTypeRef), (SCAL, element_count, unsigned), sem_vector_elem_ty(LLVMScalableVectorType, element_type) sem_vector_count(LLVMScalableVectorType, element_count))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetAlignment, LOCKED, (REF, value, VALUE, LLVMValueRef, AlignmentOwner), (SCAL, bytes, unsigned), sem_alignment(LLVMSetAlignment, value, bytes))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMSetArgOperand, LOCKED, (REF, funclet, VALUE, LLVMValueRef, FuncletPadInst), (SCAL, index, unsigned), (REF, value, VALUE, LLVMValueRef), sem_arg_operand_index(LLVMSetArgOperand, funclet, index))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetAtomicRMWBinOp, LOCKED, (REF, atomic_rmw_inst, VALUE, LLVMValueRef, AtomicRMWInst), (SCAL, bin_op, unsigned), sem_rmw_binop(LLVMSetAtomicRMWBinOp, bin_op))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetAtomicSingleThread, LOCKED, (REF, atomic_inst, VALUE, LLVMValueRef, Instruction), (SCAL, single_thread, bool))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetAtomicSyncScopeID, LOCKED, (REF, atomic_inst, VALUE, LLVMValueRef, Instruction), (SCAL, ssid, unsigned), sem_atomic_inst(LLVMSetAtomicSyncScopeID, atomic_inst))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetCleanup, LOCKED, (REF, landing_pad, VALUE, LLVMValueRef, LandingPadInst), (SCAL, val, bool))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetCmpXchgFailureOrdering, LOCKED, (REF, cmp_xchg_inst, VALUE, LLVMValueRef, AtomicCmpXchgInst), (SCAL, ordering, unsigned), sem_cmpxchg_failure_ordering(LLVMSetCmpXchgFailureOrdering, ordering))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetCmpXchgSuccessOrdering, LOCKED, (REF, cmp_xchg_inst, VALUE, LLVMValueRef, AtomicCmpXchgInst), (SCAL, ordering, unsigned), sem_cmpxchg_success_ordering(LLVMSetCmpXchgSuccessOrdering, ordering))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetCondition, LOCKED, (REF, branch, VALUE, LLVMValueRef, BranchInst), (REF, cond, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetCurrentDebugLocation, LOCKED, (REF, builder, BUILDER, LLVMBuilderRef), (REF, value, VALUE, LLVMValueRef, MDNodeValue))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetCurrentDebugLocation2, LOCKED, (REF, builder, BUILDER, LLVMBuilderRef), (REF, loc, METADATA, LLVMMetadataRef, MDNodeMetadata))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetDLLStorageClass, LOCKED, (REF, global, VALUE, LLVMValueRef, GlobalValue), (SCAL, class, unsigned))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetDataLayout, LOCKED, (REF, module, MODULE, LLVMModuleRef), (CSTR, data_layout_str), sem_data_layout(LLVMSetDataLayout, data_layout_str))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetExact, LOCKED, (REF, div_or_shr_inst, VALUE, LLVMValueRef, Instruction), (SCAL, is_exact, bool), sem_exact(LLVMSetExact, div_or_shr_inst))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetExternallyInitialized, LOCKED, (REF, global_var, VALUE, LLVMValueRef, GlobalVariable), (SCAL, is_ext_init, bool))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetFastMathFlags, LOCKED, (REF, fp_math_inst, VALUE, LLVMValueRef, Instruction), (SCAL, fmf, unsigned), sem_fpmath(LLVMSetFastMathFlags, fp_math_inst))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetFunctionCallConv, LOCKED, (REF, function, VALUE, LLVMValueRef, Function), (SCAL, call_conv, unsigned), sem_call_conv(LLVMSetFunctionCallConv, call_conv))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetGC, LOCKED, (REF, function, VALUE, LLVMValueRef, Function), (CSTR, name))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetGlobalConstant, LOCKED, (REF, global_var, VALUE, LLVMValueRef, GlobalVariable), (SCAL, is_constant, bool))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetGlobalIFuncResolver, LOCKED, (REF, i_func, VALUE, LLVMValueRef, GlobalIFunc), (REF, resolver, VALUE, LLVMValueRef, Constant))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetInitializer, LOCKED, (REF, global_var, VALUE, LLVMValueRef, GlobalVariable), (REF, constant_val, VALUE, LLVMValueRef, Constant), sem_set_initializer(LLVMSetInitializer, global_var, constant_val))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetInstDebugLocation, LOCKED, (REF, builder, BUILDER, LLVMBuilderRef), (REF, inst, VALUE, LLVMValueRef, Instruction))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMSetInstrParamAlignment, LOCKED, (REF, instr, VALUE, LLVMValueRef, CallBase), (SCAL, idx, unsigned), (SCAL, align, unsigned), sem_param_alignment(LLVMSetInstrParamAlignment, align))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetInstructionCallConv, LOCKED, (REF, instr, VALUE, LLVMValueRef, CallBase), (SCAL, call_conv, unsigned), sem_call_conv(LLVMSetInstructionCallConv, call_conv))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetIsDisjoint, LOCKED, (REF, inst, VALUE, LLVMValueRef, PossiblyDisjointInst), (SCAL, is_disjoint, bool))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetIsInBounds, LOCKED, (REF, gep, VALUE, LLVMValueRef, GetElementPtrInst), (SCAL, in_bounds, bool))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetIsNewDbgInfoFormat, LOCKED, (REF, module, MODULE, LLVMModuleRef), (SCAL, use_new_format, bool))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetLinkage, LOCKED, (REF, global, VALUE, LLVMValueRef, GlobalValue), (SCAL, linkage, unsigned))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMSetMetadata, LOCKED, (REF, val, VALUE, LLVMValueRef, Instruction), (SCAL, kind_id, unsigned), (REF, node, VALUE, LLVMValueRef, ExtractMDNodeValue))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMSetModuleIdentifier, LOCKED, (REF, module, MODULE, LLVMModuleRef), (OPTSTR, ident, len), (SCAL, len, size_t))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetModuleInlineAsm, LOCKED, (REF, module, MODULE, LLVMModuleRef), (CSTR, asm_string))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMSetModuleInlineAsm2, LOCKED, (REF, module, MODULE, LLVMModuleRef), (OPTSTR, asm_string, len), (SCAL, len, size_t))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetNNeg, LOCKED, (REF, non_neg_inst, VALUE, LLVMValueRef, Instruction), (SCAL, is_non_neg, bool), sem_nneg(LLVMSetNNeg, non_neg_inst))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetNSW, LOCKED, (REF, arith_inst, VALUE, LLVMValueRef, Instruction), (SCAL, has_nsw, bool), sem_nsw(LLVMSetNSW, arith_inst))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetNUW, LOCKED, (REF, arith_inst, VALUE, LLVMValueRef, Instruction), (SCAL, has_nuw, bool), sem_nsw(LLVMSetNUW, arith_inst))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetNormalDest, LOCKED, (REF, invoke_inst, VALUE, LLVMValueRef, InvokeInst), (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef), sem_inst_bb(LLVMSetNormalDest, invoke_inst, basic_block))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMSetOperand, LOCKED, (REF, user, VALUE, LLVMValueRef, User), (SCAL, index, unsigned), (REF, val, VALUE, LLVMValueRef), sem_operand_index(LLVMSetOperand, user, index))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetOrdering, LOCKED, (REF, memory_access_inst, VALUE, LLVMValueRef, OrderingOwner), (SCAL, ordering, unsigned), sem_owner_ordering(LLVMSetOrdering, memory_access_inst, ordering))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetParamAlignment, LOCKED, (REF, arg, VALUE, LLVMValueRef, Argument), (SCAL, align, unsigned), sem_param_alignment(LLVMSetParamAlignment, align))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetParentCatchSwitch, LOCKED, (REF, catch_pad, VALUE, LLVMValueRef, CatchPadInst), (REF, catch_switch, VALUE, LLVMValueRef, CatchSwitchInst))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetPersonalityFn, LOCKED, (REF, function, VALUE, LLVMValueRef, Function), (REF, personality_fn, VALUE, LLVMValueRef, Constant))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetPrefixData, LOCKED, (REF, function, VALUE, LLVMValueRef, Function), (REF, prefix_data, VALUE, LLVMValueRef, Constant))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetPrologueData, LOCKED, (REF, function, VALUE, LLVMValueRef, Function), (REF, prologue_data, VALUE, LLVMValueRef, Constant))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetSection, LOCKED, (REF, global, VALUE, LLVMValueRef, GlobalObject), (CSTR, section))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMSetSourceFileName, LOCKED, (REF, module, MODULE, LLVMModuleRef), (OPTSTR, name, len), (SCAL, len, size_t))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMSetSuccessor, LOCKED, (REF, term, VALUE, LLVMValueRef, Instruction), (SCAL, index, unsigned), (REF, block, BASIC_BLOCK, LLVMBasicBlockRef), sem_set_successor(LLVMSetSuccessor, term, index, block))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetTailCall, LOCKED, (REF, call_inst, VALUE, LLVMValueRef, CallInst), (SCAL, is_tail_call, bool))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetTailCallKind, LOCKED, (REF, call_inst, VALUE, LLVMValueRef, CallInst), (SCAL, kind, unsigned), sem_tail_call_kind(LLVMSetTailCallKind, kind))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetTarget, LOCKED, (REF, module, MODULE, LLVMModuleRef), (CSTR, triple))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetThreadLocal, LOCKED, (REF, global_var, VALUE, LLVMValueRef, GlobalVariable), (SCAL, is_thread_local, bool))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetThreadLocalMode, LOCKED, (REF, global_var, VALUE, LLVMValueRef, GlobalVariable), (SCAL, mode, unsigned))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetUnnamedAddr, LOCKED, (REF, global, VALUE, LLVMValueRef, GlobalValue), (SCAL, has_unnamed_addr, bool))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetUnnamedAddress, LOCKED, (REF, global, VALUE, LLVMValueRef, GlobalValue), (SCAL, unnamed_addr, unsigned))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetUnwindDest, LOCKED, (REF, invoke_inst, VALUE, LLVMValueRef, UnwindOwner), (REF, basic_block, BASIC_BLOCK, LLVMBasicBlockRef), sem_inst_bb(LLVMSetUnwindDest, invoke_inst, basic_block))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetValueName, LOCKED, (REF, val, VALUE, LLVMValueRef), (CSTR, name), sem_nameable_value(LLVMSetValueName, val, name))
FILC_LLVM_JIT_DEFINE_VOID_3(LLVMSetValueName2, LOCKED, (REF, val, VALUE, LLVMValueRef), (OPTSTR, name, name_len), (SCAL, name_len, size_t), sem_nameable_value_len(LLVMSetValueName2, val, name, name_len))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetVisibility, LOCKED, (REF, global, VALUE, LLVMValueRef, GlobalValue), (SCAL, viz, unsigned))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetVolatile, LOCKED, (REF, memory_access_inst, VALUE, LLVMValueRef, VolatileOwner), (SCAL, is_volatile, bool))
FILC_LLVM_JIT_DEFINE_VOID_2(LLVMSetWeak, LOCKED, (REF, cmp_xchg_inst, VALUE, LLVMValueRef, AtomicCmpXchgInst), (SCAL, is_weak, bool))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMSizeOf, LOCKED, VALUE, (REF, type, TYPE, LLVMTypeRef))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMStructCreateNamed, LOCKED, TYPE, (CTX, context), (CSTR, name))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMStructGetTypeAtIndex, LOCKED, TYPE, (REF, struct_ty, TYPE, LLVMTypeRef, StructTypeKind), (SCAL, index, unsigned), sem_struct_element_index(LLVMStructGetTypeAtIndex, struct_ty, index))
FILC_LLVM_JIT_DEFINE_VOID_4(LLVMStructSetBody, LOCKED, (REF, struct_ty, TYPE, LLVMTypeRef, StructTypeKind), (ARR, element_types, element_count, TYPE, LLVMTypeRef), (SCAL, element_count, unsigned), (SCAL, packed, bool), sem_struct_set_body(LLVMStructSetBody, struct_ty, element_types, element_count))
FILC_LLVM_JIT_DEFINE_REF_4(LLVMStructTypeInContext, LOCKED, TYPE, (CTX, context), (ARR, element_types, element_count, TYPE, LLVMTypeRef), (SCAL, element_count, unsigned), (SCAL, packed, bool))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMTokenTypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMTypeIsSized, LOCKED, bool, (REF, type, TYPE, LLVMTypeRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMTypeOf, LOCKED, TYPE, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMValueAsBasicBlock, LOCKED, BASIC_BLOCK, (REF, val, VALUE, LLVMValueRef, BasicBlock))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMValueAsMetadata, LOCKED, METADATA, (REF, val, VALUE, LLVMValueRef, ConstantOrArgumentOrInstructionOrMDValue))
FILC_LLVM_JIT_DEFINE_SCALAR_1(LLVMValueIsBasicBlock, LOCKED, bool, (REF, val, VALUE, LLVMValueRef))
FILC_LLVM_JIT_DEFINE_SCALAR_2(LLVMValueMetadataEntriesGetKind, LOCKED, unsigned, (REF, entries, METADATA_ENTRIES, LLVMValueMetadataEntry*), (SCAL, index, unsigned), sem_entries_index(LLVMValueMetadataEntriesGetKind, entries, index))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMValueMetadataEntriesGetMetadata, LOCKED, METADATA, (REF, entries, METADATA_ENTRIES, LLVMValueMetadataEntry*), (SCAL, index, unsigned), sem_entries_index(LLVMValueMetadataEntriesGetMetadata, entries, index))
FILC_LLVM_JIT_DEFINE_REF_2(LLVMVectorType, LOCKED, TYPE, (REF, element_type, TYPE, LLVMTypeRef), (SCAL, element_count, unsigned), sem_vector_elem_ty(LLVMVectorType, element_type) sem_vector_count(LLVMVectorType, element_count))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMVoidTypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMX86AMXTypeInContext, LOCKED, TYPE, (CTX, context))
FILC_LLVM_JIT_DEFINE_REF_1(LLVMX86FP80TypeInContext, LOCKED, TYPE, (CTX, context))

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
