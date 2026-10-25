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

/* Fil-C runtime wrappers for the LLVM JIT API (user declarations live in the
   generated user header; the ABI table lives in filc_llvm_jit_api.h).

   Every LLVM*Ref / FilLLVMJITRef / FilLLVMJITModuleRef value that crosses
   into Fil-C code is wrapped in a FILC_SPECIAL_TYPE_LLVM_JIT special object
   (payload structs are declared in filc_runtime.h). This file implements:

   - lazy dlopen of libfilcllvmjit.so (see filc_llvm_jit_ensure_api), whose
     single export FilLLVMJIT_GetApi returns the FilLLVMJITApi function
     pointer table (API v4, no Initialize entry and no global-context creators:
     first use lazily boots LLVM
     targets and the 128MB heap mapping via GetHeapMemory/Create).
     libpizlo never links LLVM; all calls go through the
     table. The 128MB RWX jit_heap boot is performed lazily by the shim's
     internal ensure-initialized path (which hands the range to
     jit_heap_add_fresh_memory when that
     symbol is visible, and always reports it via GetHeapMemory); the
     runtime feeds the range to the JIT heap itself when the shim could not
     (i.e. when the symbol is not visible from the shim). Fil-C builds always
     enable the JIT heap (PAS_ENABLE_JIT=1, enforced above), so the feeding
     path in the init slow path is always live and a failed JIT-heap probe
     panics instead of silently running without jit_heap backing.
   - the object model (context lock, refcounts, keep-alives, destructors).
   - native wrappers (filc_native_*, exposed to Fil-C under their bare
     <C name>: LLVM* and FilLLVMJIT_*) for the whole curated API. Thin
     per-function wrappers are mechanically emitted for the curated API
     (see the generator comment at the bottom of this file); tricky functions
     (arrays, out-params, the FilLLVMJIT* session API, resolve) are
     hand-written.
   - lazy extern resolution: filc_llvm_jit_resolve /
     filc_llvm_jit_resolve_raw_pointer / zexternptr plus the pizlonated
     natives FilLLVMJIT_Resolve / FilLLVMJIT_ResolveRawPointer, with resolved
     flight pointers recorded as GC roots in the generated-module object.
   - gating on filc_allow_jit (set by filc_start_program_with_jit for
     processes launched with JIT support).

   Conventions for Fil-C callers (also documented per-function where the
   thin wrappers are generated):
   - Opaque LLVM*Ref / FilLLVMJITRef values are passed and returned as plain
     Fil-C pointers (the wrapper objects). A null Fil-C pointer maps to a
     NULL LLVM handle (used pervasively by the C API for "clear"/"default").
   - const char* parameters are Fil-C strings (null means NULL), except
     where the C API requires non-null (then null panics).
   - T* input arrays (LLVMValueRef*, LLVMTypeRef*, LLVMBasicBlockRef*,
     LLVMMetadataRef*, LLVMAttributeRef*) are Fil-C pointers to arrays of
     wrapped Fil-C pointers (8-byte stride, i.e. ordinary Fil-C ptr
     arrays); the count parameter is the element count. Raw-word arrays
     (uint64_t Words[] etc.) are Fil-C pointers to raw 8-byte (or 4-byte
     for unsigned IntParams[]) slots.
   - Functions whose C form writes through an out-array (LLVMGetParams,
     LLVMGetBasicBlocks, ...) take a Fil-C pointer to a preallocated array
     of Fil-C pointer slots; this file queries the companion count function
     itself, wraps each returned handle, and stores the wrappers with checked
     stores (under-allocation panics cleanly instead of corrupting memory).
   - Enums and LLVMBool cross as unsigned and bool; uint64_t crosses as
     unsigned long long; uint8_t crosses as unsigned.
   - FilLLVMJIT_GetFunctionAddress/GetGlobalValueAddress/GetPointerToGlobal
     return the raw JITed address carried by a forged ( capability-less)
     Fil-C pointer. It is an opaque token: it cannot be dereferenced from
     Fil-C. The JITed code follows the pizlonated ABI and is meant to be
     consumed through the Fil-C runtime, not called directly.
 */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "filc_runtime.h"

#if PAS_ENABLE_FILC

#include "filc_native.h"
#include "filc_runtime_inlines.h"
#include "fugc.h"
#include "../../../filc/llvmjit/filc_llvm_jit_api.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* Fil-C builds always boot the JIT heap (see the PAS_FILC PAS_ENABLE_JIT
   setting in pas_config.h); fail the build otherwise so JIT init below can
   rely on jit_heap_add_fresh_memory/jit_heap_try_allocate unconditionally. */
#if !PAS_ENABLE_JIT
#error "Fil-C LLVM JIT support requires PAS_ENABLE_JIT=1."
#endif
#include "jit_heap.h"

/* Set by filc_start_program_with_jit for processes launched
   with JIT support. Every wrapper below panics unless this is true. */
bool filc_allow_jit = false;

static const struct FilLLVMJITApi* filc_llvm_jit_api = NULL;
PAS_DECLARE_LOCK(filc_llvm_jit_api);
PAS_DEFINE_LOCK(filc_llvm_jit_api);
static bool filc_llvm_jit_did_init = false;
/* The shim is intentionally never dlclosed: JITed code and control blocks
   can outlive any particular wrapper call. */
static void* filc_llvm_jit_so_handle = NULL;

/* Pinned process-global state (objects are immortal: each is pushed onto
   filc_global_variable_root_ptrs when installed -- the array the collector
   actually marks, so the raw pointers cached here stay alive forever;
   replaced values leak by design, which is fine because JIT sessions,
   generated modules, resolvers, and tables are process-lifetime resources
   in API v1, which has no unload). NOTE: this must be root_ptrs, not
   filc_global_variable_roots: the latter is scanned without setting mark
   bits (it is only for non-GC global objects), so GC-allocated pins placed
   there would never be marked -- the collector would sweep them and the
   verify mode would (correctly) report them as unmarked.

   Every pin below lands in root_ptrs, and root_ptrs is
   marked in ALL GC configurations because all marking funnels through
   filc_mark_global_roots (filc_runtime_inlines.h), which marks each entry
   with whatever marker drives the collection: the concurrent FUGC_MARKER
   path (fugc.c mark_and_start_marking), the STW path (the same marking code
   with the world stopped -- FUGC_STW=1 changes only thread coordination, not
   the root set), and the VERIFY_MARKER path (fugc.c verify_marking under
   FUGC_VERIFY=1, including verify-early). Scribbling (FUGC_SCRIBBLE=1) only
   scribbles objects still unmarked after that marking, so pinned objects are
   never scribbled. The jitrootstress test forces
   collections (zgc_request_and_wait plus allocation churn) in every harness
   mode -- rage, scribble+verify, STW, release -- which would trip verify or
   crash on any unmarked pin. */
static filc_ptr filc_llvm_jit_current_session_ref = { NULL, NULL };
static filc_ptr filc_llvm_jit_current_module_ref = { NULL, NULL };
static filc_ptr filc_llvm_jit_resolver_ref = { NULL, NULL };
static filc_ptr filc_llvm_jit_resolver_context_ref = { NULL, NULL };
static filc_ptr filc_llvm_jit_zexact_table_ref = { NULL, NULL };

static void filc_llvm_jit_pin_global(filc_thread* my_thread, filc_ptr value)
{
    if (filc_ptr_is_totally_null(value))
        return;
    if (!filc_ptr_is_markable(value))
        return;
    filc_global_variable_roots_lock_lock();
    /* root_ptrs, not roots: this array is marked by the collector (see the
       file comment above); roots is only scanned, which leaves GC-allocated
       pins unmarked and sweepable. */
    filc_object_array_push(&filc_global_variable_root_ptrs, filc_ptr_object(value));
    filc_global_variable_roots_lock_unlock();
    /* Keep the tracker happy: the value is now a global root. */
    PAS_UNUSED_PARAM(my_thread);
}

static const struct FilLLVMJITApi* filc_llvm_jit_ensure_api_slow(filc_thread* my_thread);

static const struct FilLLVMJITApi* filc_llvm_jit_ensure_api(filc_thread* my_thread)
{
    /* The gate lives here (not just in the slow path) so that every wrapper
       panics when JIT support is not enabled, even after a successful init
       in some earlier phase of the process. */
    FILC_CHECK(filc_allow_jit, NULL,
               "Fil-C LLVM JIT API used but JIT support is not enabled for this process "
               "(launch with JIT support, e.g. via -enable-filc-jit).");
    const struct FilLLVMJITApi* api = filc_llvm_jit_api;
    if (api)
        return api;
    return filc_llvm_jit_ensure_api_slow(my_thread);
}

static bool filc_llvm_jit_try_load_one(const char* path)
{
    if (!path || !path[0])
        return false;
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle)
        return false;
    const struct FilLLVMJITApi* (*get_api)(void) =
        (const struct FilLLVMJITApi* (*)(void))dlsym(handle, "FilLLVMJIT_GetApi");
    if (!get_api) {
        dlclose(handle);
        return false;
    }
    const struct FilLLVMJITApi* api = get_api();
    /* API v4 removed the global-context creators (lazy auto-init already
       removed Initialize in v3), shifting every later field: only v4 tables
       match this runtime's compiled-in layout. */
    if (!api || api->version != FILC_LLVM_JIT_API_VERSION) {
        dlclose(handle);
        return false;
    }
    filc_llvm_jit_api = api;
    filc_llvm_jit_so_handle = handle;
    return true;
}

static const struct FilLLVMJITApi* filc_llvm_jit_ensure_api_slow(filc_thread* my_thread)
{
    pas_lock_lock(&filc_llvm_jit_api_lock);
    if (filc_llvm_jit_did_init) {
        const struct FilLLVMJITApi* api = filc_llvm_jit_api;
        pas_lock_unlock(&filc_llvm_jit_api_lock);
        PAS_ASSERT(api);
        return api;
    }

    /* NOTE: dlopen plus the lazy one-time init below run under the api lock
       but with the thread exited, so a concurrent GC never blocks on us. */
    filc_exit(my_thread);

    /* 1. Explicit override for tests and exotic layouts. */
    const char* env = getenv("FILC_LLVMJIT_SO");
    if (env && env[0])
        filc_llvm_jit_try_load_one(env);

    /* 2. ../libexec relative to libpizlo, located via dladdr on our own
       address (libpizlo lives at $prefix/lib/libpizlo.so, the shim at
       $prefix/libexec/libfilcllvmjit.so). */
    if (!filc_llvm_jit_api) {
        Dl_info info;
        if (dladdr((void*)filc_llvm_jit_ensure_api_slow, &info) && info.dli_fname) {
            char dir[4096];
            snprintf(dir, sizeof(dir), "%s", info.dli_fname);
            char* slash = strrchr(dir, '/');
            if (slash) {
                *slash = '\0';
                char candidate[4096];
                snprintf(candidate, sizeof(candidate), "%s/../libexec/libfilcllvmjit.so", dir);
                if (!filc_llvm_jit_try_load_one(candidate)) {
                    snprintf(candidate, sizeof(candidate),
                             "%s/libexec/libfilcllvmjit.so", dir);
                    filc_llvm_jit_try_load_one(candidate);
                }
            }
        }
    }

    /* 3. pizfix/FHS fallback. */
    if (!filc_llvm_jit_api)
        filc_llvm_jit_try_load_one("/opt/filc/libexec/libfilcllvmjit.so");

    if (!filc_llvm_jit_api) {
        const char* error = dlerror();
        filc_enter(my_thread);
        pas_lock_unlock(&filc_llvm_jit_api_lock);
        filc_safety_panic(NULL, "could not dlopen libfilcllvmjit.so: %s.",
                          error ? error : "<unknown>");
    }

    const struct FilLLVMJITApi* api = filc_llvm_jit_api;

    /* Lazy automatic one-time init (no user-visible Initialize in API v4).
       GetHeapMemory boots LLVM targets and the 128MB heap mapping inside the
       shim on first use; the runtime feeds that range to jit_heap itself
       exactly when the shim could not (i.e. when jit_heap_add_fresh_memory
       is not visible from the shim; visibility is identical here). Exactly
       one side feeds: handing the same range to the heap twice would corrupt
       it. Fil-C builds always enable the JIT heap (PAS_ENABLE_JIT=1,
       enforced at the top of this file), so there is no silent fallback to
       running without jit_heap backing. */
    {
        void* base = NULL;
        unsigned long long size = 0;
        if (!api->GetHeapMemory || api->GetHeapMemory(&base, &size) || !base || !size) {
            const char* error = api->GetLastError ? api->GetLastError() : "<unknown>";
            filc_enter(my_thread);
            pas_lock_unlock(&filc_llvm_jit_api_lock);
            filc_safety_panic(NULL, "Fil-C LLVM JIT lazy init failed: %s.", error);
        }
        if (!dlsym(RTLD_DEFAULT, "jit_heap_add_fresh_memory")) {
            pas_range range =
                pas_range_create((uintptr_t)base, (uintptr_t)base + (size_t)size);
            jit_heap_add_fresh_memory(range);
        }
    }
    /* Prove the JIT heap is live before any wrapper can rely on it. A failed
       round-trip means the heap has no backing memory, which must never
       happen now that Fil-C builds require PAS_ENABLE_JIT=1. */
    {
        void* probe = jit_heap_try_allocate(16);
        if (!probe) {
            filc_enter(my_thread);
            pas_lock_unlock(&filc_llvm_jit_api_lock);
            filc_safety_panic(NULL, "Fil-C LLVM JIT heap has no usable memory.");
        }
        jit_heap_deallocate(probe);
    }

    filc_enter(my_thread);
    filc_llvm_jit_did_init = true;
    pas_lock_unlock(&filc_llvm_jit_api_lock);
    return api;
}

/* ------------------------------------------------------------------ */
/* Object model helpers. */


/* Common tail of every parented-object creation. The caller passes the
   context state (never NULL) and, for
   module members, the module state (or NULL). Bumps parent refcounts with
   atomics and installs GC keep-alives to the context/module *objects* (the
   objects may die before this payload does; the immortal states in
   state/module_state are what the destructor relies on). */
static filc_ptr filc_llvm_jit_keepalive(filc_thread* my_thread, void* payload)
{
    filc_ptr ref = filc_ptr_create_with_special_object_and_manual_tracking(
        filc_object_for_special_payload(payload));
    filc_thread_track_object(my_thread, filc_ptr_object(ref));
    return ref;
}

static filc_ptr filc_llvm_jit_publish(filc_thread* my_thread, filc_object* object)
{
    return filc_ptr_create_with_special_object(my_thread, object);
}

/* Wrap a raw handle produced by the API table. The donors are payloads:
   context_obj is the context payload (never NULL for a live handle),
   module_obj is a module payload (or NULL). Donor keep-alives are
   re-created (tracked) for the new object; donor refcounts are bumped with
   atomics. Returns a null Fil-C pointer for a NULL handle. */
/* Refcount drops. The last dropper performs the LLVM disposal and frees
   the control block, which yields the only legal teardown order (handles,
   then modules, then contexts) regardless of the order the collector
   destructs the payloads. No locks: atomics only. */
static void filc_llvm_jit_drop_context_state(filc_llvm_jit_context_state* state)
{
    if (!state)
        return;
    if (__atomic_fetch_sub(&state->ref_count, 1u, __ATOMIC_SEQ_CST) != 1)
        return;
    const struct FilLLVMJITApi* api = filc_llvm_jit_api;
    if (state->handle && api && api->ContextDispose)
        api->ContextDispose((LLVMContextRef)state->handle);
    free(state);
}

static void filc_llvm_jit_drop_module_state(filc_llvm_jit_module_state* module_state)
{
    if (!module_state)
        return;
    if (__atomic_fetch_sub(&module_state->ref_count, 1u, __ATOMIC_SEQ_CST) != 1)
        return;
    const struct FilLLVMJITApi* api = filc_llvm_jit_api;
    /* The JIT compiles a bitcode copy (see shim_AddModule), so disposing
       the original here never pulls code out from under JITed callers. */
    if (api && api->DisposeModule)
        api->DisposeModule((LLVMModuleRef)module_state->handle);
    filc_llvm_jit_drop_context_state(module_state->context_state);
    free(module_state);
}

static filc_ptr filc_llvm_jit_wrap(filc_thread* my_thread,
                                   filc_llvm_jit_context_object* context_obj,
                                   filc_llvm_jit_object* module_obj,
                                   filc_llvm_jit_kind kind,
                                   void* handle)
{
    size_t size;
    switch (kind) {
    case FILC_LLVM_JIT_KIND_MODULE:
        size = sizeof(filc_llvm_jit_module_object);
        break;
    case FILC_LLVM_JIT_KIND_METADATA_ENTRIES:
    case FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES:
        size = sizeof(filc_llvm_jit_entries_object);
        break;
    case FILC_LLVM_JIT_KIND_TYPE:
    case FILC_LLVM_JIT_KIND_VALUE:
    case FILC_LLVM_JIT_KIND_BASIC_BLOCK:
    case FILC_LLVM_JIT_KIND_METADATA:
    case FILC_LLVM_JIT_KIND_NAMED_METADATA:
    case FILC_LLVM_JIT_KIND_USE:
    case FILC_LLVM_JIT_KIND_DBG_RECORD:
    case FILC_LLVM_JIT_KIND_ATTRIBUTE:
    case FILC_LLVM_JIT_KIND_OPERAND_BUNDLE:
    case FILC_LLVM_JIT_KIND_BUILDER:
        size = sizeof(filc_llvm_jit_value_object);
        break;
    default:
        filc_safety_panic(NULL, "filc_llvm_jit_wrap: unexpected kind %u.", (unsigned)kind);
    }
    if (!handle)
        return filc_ptr_forge_null();
    FILC_CHECK(context_obj, NULL,
               "Fil-C LLVM JIT API: null LLVM context (the global-context API "
               "was removed; pass an explicit context and use the ...InContext form).");
    filc_llvm_jit_context_state* context_state = context_obj->state;
    filc_ptr context_ref = filc_llvm_jit_keepalive(my_thread, context_obj);
    __atomic_fetch_add(&context_state->ref_count, 1u, __ATOMIC_SEQ_CST);
    filc_llvm_jit_module_state* module_state = NULL;
    filc_ptr module_ref = filc_ptr_forge_null();
    if (kind == FILC_LLVM_JIT_KIND_MODULE) {
        module_state =
            (filc_llvm_jit_module_state*)malloc(sizeof(filc_llvm_jit_module_state));
        if (!module_state)
            filc_safety_panic(NULL, "out of memory wrapping LLVM module.");
        module_state->context_state = context_state;
        __atomic_fetch_add(&context_state->ref_count, 1u, __ATOMIC_SEQ_CST);
        module_state->handle = handle;
        __atomic_store_n(&module_state->ref_count, 1u, __ATOMIC_SEQ_CST);
    } else if (module_obj) {
        module_state = module_obj->module_state;
        __atomic_fetch_add(&module_state->ref_count, 1u, __ATOMIC_SEQ_CST);
        module_ref = filc_llvm_jit_keepalive(my_thread, module_obj);
    }
    filc_object* object = filc_allocate_special_with_heap(
        my_thread, size, 16, FILC_SPECIAL_TYPE_LLVM_JIT, fugc_destructor_heap);
    filc_llvm_jit_object* payload = (filc_llvm_jit_object*)filc_object_lower(object);
    payload->kind = kind;
    payload->state = context_state;
    payload->context_ref = context_ref;
    payload->module_state = module_state;
    payload->module_ref = module_ref;
    payload->handle = handle;
    if (kind == FILC_LLVM_JIT_KIND_METADATA_ENTRIES
        || kind == FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES)
        ((filc_llvm_jit_entries_object*)payload)->count = 0;
    return filc_llvm_jit_publish(my_thread, object);
}


/* Wrap a raw handle produced by the API table. Returns a null Fil-C pointer
   for a NULL handle. The caller passes the context object (never NULL for
   a live handle) plus the module object (or NULL), and this installs GC
   keep-alives to both. */


/* Builder adoption: a builder positioned in a block (or otherwise placed
   into a module) keeps that module alive. Drops the previous adoption.
   Runs under the context state's lock with the whole chain alive. */
static void filc_llvm_jit_builder_adopt_module(
    filc_thread* my_thread, filc_llvm_jit_object* builder,
    filc_llvm_jit_object* module_obj)
{
    PAS_ASSERT(builder->kind == FILC_LLVM_JIT_KIND_BUILDER);
    filc_llvm_jit_module_state* new_state =
        module_obj ? module_obj->module_state : NULL;
    if (builder->module_state == new_state)
        return;
    if (builder->module_state)
        filc_llvm_jit_drop_module_state(builder->module_state);
    builder->module_state = new_state;
    /* NOTE: the marker never reads module_state (only module_ref), so the
       raw store above needs no guard; readers of module_state always hold the
       context lock. */
    if (new_state) {
        __atomic_fetch_add(&new_state->ref_count, 1u, __ATOMIC_SEQ_CST);
        /* Publish for the lock-free marker (which loads this flight with
           filc_flight_ptr_load_with_manual_tracking and marks the copy),
           plus a Steele barrier marking the adopted module so an adoption
           that races a concurrent scan cannot strand an unmarked target. */
        filc_ptr keepalive = filc_llvm_jit_keepalive(my_thread, module_obj);
        filc_flight_ptr_store_atomic(my_thread, &builder->module_ref, keepalive);
    } else
        filc_flight_ptr_store_atomic(my_thread, &builder->module_ref,
                                     filc_ptr_forge_null());
}


/* Unwrap a Fil-C pointer to a parented LLVM JIT object. Null maps to NULL
   (pervasive "clear"/"default" convention of the C API). Otherwise checks the
   special type and kind, and folds the object's context/module into the
   caller-provided donors (first non-null wins; all non-null contexts must
   agree). Returns the payload, or NULL for null input. */
/* Unwrap a Fil-C pointer to a parented LLVM JIT object. Null maps to NULL
   (pervasive "clear"/"default" convention of the C API). Otherwise checks
   the special type and kind, and folds the object's context/module donors
   into the caller-provided slots (first non-null wins; all non-null
   contexts must agree). Returns the payload, or NULL for null input. */
static filc_llvm_jit_object* filc_llvm_jit_unwrap_obj(
    filc_thread* my_thread, filc_ptr ptr, filc_llvm_jit_kind expected,
    filc_llvm_jit_context_object** context_inout,
    filc_llvm_jit_object** module_inout)
{
    PAS_UNUSED_PARAM(my_thread);
    if (filc_ptr_is_totally_null(ptr))
        return NULL;
    filc_check_access_special(ptr, FILC_SPECIAL_TYPE_LLVM_JIT);
    filc_llvm_jit_object* payload = (filc_llvm_jit_object*)filc_ptr_ptr(ptr);
    FILC_CHECK(payload->kind == expected, NULL,
               "Fil-C LLVM JIT API: object kind mismatch (expected %u, got %u).",
               (unsigned)expected, (unsigned)payload->kind);
    filc_llvm_jit_context_object* context_obj =
        (filc_llvm_jit_context_object*)filc_ptr_ptr(payload->context_ref);
    if (!*context_inout)
        *context_inout = context_obj;
    else {
        FILC_CHECK((*context_inout)->state == payload->state, NULL,
                   "Fil-C LLVM JIT API: mixing objects from different LLVM contexts.");
    }
    if (payload->module_state && !*module_inout) {
        if (payload->kind == FILC_LLVM_JIT_KIND_MODULE)
            *module_inout = payload;
        else {
            *module_inout =
                (filc_llvm_jit_object*)filc_ptr_ptr(payload->module_ref);
        }
    }
    return payload;
}


static filc_llvm_jit_context_object* filc_llvm_jit_unwrap_context(
    filc_thread* my_thread, filc_ptr ptr)
{
    PAS_UNUSED_PARAM(my_thread);
    FILC_CHECK(!filc_ptr_is_totally_null(ptr), NULL,
               "Fil-C LLVM JIT API: null LLVM context.");
    filc_check_access_special(ptr, FILC_SPECIAL_TYPE_LLVM_JIT);
    filc_llvm_jit_context_object* payload = (filc_llvm_jit_context_object*)filc_ptr_ptr(ptr);
    FILC_CHECK(payload->kind == FILC_LLVM_JIT_KIND_CONTEXT, NULL,
               "Fil-C LLVM JIT API: expected an LLVM context object.");
    return payload;
}

static filc_llvm_jit_session_object* filc_llvm_jit_unwrap_session(
    filc_thread* my_thread, filc_ptr ptr)
{
    PAS_UNUSED_PARAM(my_thread);
    FILC_CHECK(!filc_ptr_is_totally_null(ptr), NULL,
               "Fil-C LLVM JIT API: null JIT session.");
    filc_check_access_special(ptr, FILC_SPECIAL_TYPE_LLVM_JIT);
    filc_llvm_jit_session_object* payload = (filc_llvm_jit_session_object*)filc_ptr_ptr(ptr);
    FILC_CHECK(payload->kind == FILC_LLVM_JIT_KIND_JIT_SESSION, NULL,
               "Fil-C LLVM JIT API: expected a JIT session object.");
    return payload;
}

static filc_llvm_jit_generated_module_object* filc_llvm_jit_unwrap_generated_module(
    filc_thread* my_thread, filc_ptr ptr)
{
    PAS_UNUSED_PARAM(my_thread);
    if (filc_ptr_is_totally_null(ptr))
        return NULL;
    filc_check_access_special(ptr, FILC_SPECIAL_TYPE_LLVM_JIT);
    filc_llvm_jit_generated_module_object* payload =
        (filc_llvm_jit_generated_module_object*)filc_ptr_ptr(ptr);
    FILC_CHECK(payload->kind == FILC_LLVM_JIT_KIND_GENERATED_MODULE, NULL,
               "Fil-C LLVM JIT API: expected a JIT generated-module object.");
    return payload;
}

static void* filc_llvm_jit_handle_of(filc_llvm_jit_object* payload)
{
    return payload ? payload->handle : NULL;
}


/* Decode a Fil-C array of wrapped pointers into a tmp buffer of raw handles.
   Null input maps to NULL (caller passes the element count separately). */
/* Decode a Fil-C array of wrapped pointers into a tmp buffer of raw handles.
   Null input maps to NULL (caller passes the element count separately). */
static void** filc_llvm_jit_decode_handle_array(
    filc_thread* my_thread, filc_ptr array_ptr, size_t count,
    filc_llvm_jit_kind expected,
    filc_llvm_jit_context_object** context_inout,
    filc_llvm_jit_object** module_inout)
{
    size_t index;
    void** result;
    if (filc_ptr_is_totally_null(array_ptr)) {
        FILC_CHECK(!count, NULL,
                   "Fil-C LLVM JIT API: null array with nonzero count.");
        return NULL;
    }
    result = (void**)filc_bmalloc_allocate_tmp(
        my_thread, filc_mul_size(count, sizeof(void*)));
    for (index = 0; index < count; ++index) {
        /* NOTE: Fil-C pointers occupy 8 bytes in Fil-C memory (the capability
           lives in a side table); only the runtime's filc_ptr is 16 bytes. */
        filc_ptr slot =
            filc_ptr_with_offset(array_ptr, filc_mul_size(index, sizeof(void*)));
        filc_check_read(slot, sizeof(void*));
        filc_ptr element = filc_load_ptr(my_thread, array_ptr,
                                         (ptrdiff_t)filc_mul_size(index, sizeof(void*)));
        filc_llvm_jit_object* payload = filc_llvm_jit_unwrap_obj(
            my_thread, element, expected, context_inout, module_inout);
        result[index] = filc_llvm_jit_handle_of(payload);
    }
    return result;
}


/* ------------------------------------------------------------------ */
/* Destruction. Runs on the collector's destructor path; takes no locks:
   a dead object is unreachable, so no live operation can observe the
   teardown, and refcounts use atomics. */

void filc_llvm_jit_destruct(void* raw_payload)
{
    filc_llvm_jit_kind kind = *(filc_llvm_jit_kind*)raw_payload;
    switch (kind) {
    case FILC_LLVM_JIT_KIND_CONTEXT: {
        filc_llvm_jit_context_object* object = (filc_llvm_jit_context_object*)raw_payload;
        filc_llvm_jit_drop_context_state(object->state);
        return;
    }
    case FILC_LLVM_JIT_KIND_MODULE: {
        filc_llvm_jit_module_object* object = (filc_llvm_jit_module_object*)raw_payload;
        filc_llvm_jit_drop_module_state(object->base.module_state);
        filc_llvm_jit_drop_context_state(object->base.state);
        return;
    }
    case FILC_LLVM_JIT_KIND_BUILDER:
    case FILC_LLVM_JIT_KIND_TYPE:
    case FILC_LLVM_JIT_KIND_VALUE:
    case FILC_LLVM_JIT_KIND_BASIC_BLOCK:
    case FILC_LLVM_JIT_KIND_METADATA:
    case FILC_LLVM_JIT_KIND_NAMED_METADATA:
    case FILC_LLVM_JIT_KIND_USE:
    case FILC_LLVM_JIT_KIND_DBG_RECORD:
    case FILC_LLVM_JIT_KIND_ATTRIBUTE:
    case FILC_LLVM_JIT_KIND_OPERAND_BUNDLE:
    case FILC_LLVM_JIT_KIND_METADATA_ENTRIES:
    case FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES: {
        const struct FilLLVMJITApi* api = filc_llvm_jit_api;
        filc_llvm_jit_object* object = (filc_llvm_jit_object*)raw_payload;
        if (kind == FILC_LLVM_JIT_KIND_BUILDER && api && api->DisposeBuilder)
            api->DisposeBuilder((LLVMBuilderRef)object->handle);
        /* Entries arrays are never freed: API v1 exposes no Dispose entry
           point for them (mirrors the header, which omits LLVMDispose*). */
        filc_llvm_jit_drop_module_state(object->module_state);
        filc_llvm_jit_drop_context_state(object->state);
        return;
    }
    case FILC_LLVM_JIT_KIND_JIT_SESSION: {
        /* Sessions own the whole JIT; API v1 has no teardown (process
           lifetime). Nothing to do. */
        return;
    }
    case FILC_LLVM_JIT_KIND_GENERATED_MODULE: {
        filc_llvm_jit_generated_module_object* object =
            (filc_llvm_jit_generated_module_object*)raw_payload;
        size_t retired_index;
        /* Only the retired buffers are freed, and only here: growth never
           frees a live buffer (see filc_llvm_jit_record_root), so no
           lock-free marker snapshot can be iterating freed memory. The
           destructor runs only for a dead object, which no marker can still
           reach, so no lock is needed. */
        free(object->roots);
        object->roots = NULL;
        object->num_roots = 0;
        object->roots_capacity = 0;
        for (retired_index = object->num_retired; retired_index--;)
            free(object->retired_roots[retired_index]);
        free(object->retired_roots);
        object->retired_roots = NULL;
        object->num_retired = 0;
        object->retired_capacity = 0;
        return;
    }
    default:
        PAS_ASSERT(!"filc_llvm_jit_destruct: bad kind");
        return;
    }
}


/* ------------------------------------------------------------------ */
/* Current session/module tracking + root recording. */

static void filc_llvm_jit_set_current(filc_thread* my_thread,
                                      filc_ptr session_ref,
                                      filc_ptr module_ref)
{
    /* Pin BEFORE publishing. The pins land in
       filc_global_variable_root_ptrs (the array the collector actually
       marks); the stores below only make the refs reachable through the
       globals. Publishing first would open a window where a lock-free reader
       sees a live-but-unpinned module. Pinning first is idempotent (a
       duplicate array entry just marks the same object twice), so callers
       that already pinned (e.g. AddModule pins before its session publish)
       stay correct. The pins run before the api lock is taken; that is safe
       because no path ever acquires api_lock while holding the global-roots
       lock (the marker takes only the latter), so no lock-order inversion is
       possible. */
    if (!filc_ptr_is_totally_null(session_ref))
        filc_llvm_jit_pin_global(my_thread, session_ref);
    if (!filc_ptr_is_totally_null(module_ref))
        filc_llvm_jit_pin_global(my_thread, module_ref);
    pas_lock_lock(&filc_llvm_jit_api_lock);
    if (!filc_ptr_is_totally_null(session_ref))
        filc_llvm_jit_current_session_ref = session_ref;
    if (!filc_ptr_is_totally_null(module_ref))
        filc_llvm_jit_current_module_ref = module_ref;
    pas_lock_unlock(&filc_llvm_jit_api_lock);
}

/* Pin a resolved flight pointer so the collector keeps it alive. JITed code
   keeps resolved addresses in its own globals, which the collector cannot
   see, so the generated-module object must root them. Falls back to the
   process-global variable roots when there is no module (always correct,
   immortal). Module roots slots accept any filc_ptr (null/forged entries
   are simply skipped by the marker).

   Lock-free-marker protocol: the writer (here) runs under module->lock and
   publishes roots/num_roots through the roots_generation seqlock (odd,
   payload with release, even). Growth NEVER frees the old buffer: a
   concurrent lock-free marker snapshot may still be iterating it, so the
   superseded buffer is stashed in retired_roots and reclaimed only by the
   destructor of the dead object. Retired growth is bounded, not a leak:
   capacity doubles per growth, so the retired count
   is O(log N) and the retired bytes are a halving series under ~2x live.
   Snapshots are therefore always consistent prefixes of the true set, and no
   snapshot buffer is ever unmapped under the marker. After publishing, the
   writer issues a Steele insertion barrier on the new root itself: if a
   collection is marking concurrently and its scan of this module took an
   older prefix that misses this root, the barrier still marks the root, so
   the same cycle keeps it alive no matter when the scan ran (barriering the
   already-marked module would be a no-op, not a re-grey, since barrier_impl
   does not re-push marked objects). No missed live roots, no blocking locks
   in the marker, no reclamation race. The collector
   is non-moving, so the marker's free-singleton repoints need no write-back
   here (see filc_llvm_jit_mark_outgoing_ptrs); marking is idempotent. */
static void filc_llvm_jit_record_root(
    filc_thread* my_thread, filc_llvm_jit_generated_module_object* module, filc_ptr resolved)
{
    if (!module) {
        if (filc_ptr_is_markable(resolved)) {
            filc_global_variable_roots_lock_lock();
            /* root_ptrs (marked), not roots (scanned only): see pin_global. */
            filc_object_array_push(&filc_global_variable_root_ptrs, filc_ptr_object(resolved));
            filc_global_variable_roots_lock_unlock();
        }
        return;
    }
    pas_lock_lock(&module->lock);
    unsigned generation = module->roots_generation;
    PAS_ASSERT(!(generation & 1u));
    __atomic_store_n(&module->roots_generation, generation + 1u, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    if (module->num_roots == module->roots_capacity) {
        size_t new_capacity = module->roots_capacity ? module->roots_capacity * 2 : 4;
        filc_ptr* new_roots =
            (filc_ptr*)malloc(new_capacity * sizeof(filc_ptr));
        if (!new_roots) {
            __atomic_store_n(&module->roots_generation, generation + 2u,
                             __ATOMIC_RELEASE);
            pas_lock_unlock(&module->lock);
            filc_safety_panic(NULL, "out of memory recording JIT resolve root.");
        }
        memcpy(new_roots, module->roots, module->num_roots * sizeof(filc_ptr));
        if (module->roots) {
            /* Never free the superseded buffer synchronously: a lock-free
               marker snapshot may be iterating it right now. Retire it for
               the destructor (dead object: no marker can reach it). */
            if (module->num_retired == module->retired_capacity) {
                size_t new_retired_capacity =
                    module->retired_capacity ? module->retired_capacity * 2 : 4;
                filc_ptr** new_retired = (filc_ptr**)realloc(
                    module->retired_roots, new_retired_capacity * sizeof(filc_ptr*));
                if (!new_retired) {
                    free(new_roots);
                    __atomic_store_n(&module->roots_generation, generation + 2u,
                                     __ATOMIC_RELEASE);
                    pas_lock_unlock(&module->lock);
                    filc_safety_panic(NULL, "out of memory recording JIT resolve root.");
                }
                module->retired_roots = new_retired;
                module->retired_capacity = new_retired_capacity;
            }
            module->retired_roots[module->num_retired++] = module->roots;
        }
        module->roots = new_roots;
        module->roots_capacity = new_capacity;
    }
    module->roots[module->num_roots++] = resolved;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    __atomic_store_n(&module->roots_generation, generation + 2u, __ATOMIC_RELEASE);
    pas_lock_unlock(&module->lock);
    /* Insertion barrier for a concurrently marking collector (see above).
       Steele-style: mark the newly published root itself, so the same cycle
       keeps it alive no matter when the module's own scan ran (before the
       publish with an older prefix, or after -- either way the root is
       marked). Note this must target the root, not the module: the module is
       already marked, and barrier_impl deliberately does not re-push marked
       objects, so barriering the module would be a no-op rather than a
       re-grey. This barrier exists because a forced collection racing the
       resolve storm caught verify reporting roots the module scan had
       missed. */
    if (filc_ptr_is_markable(resolved))
        filc_store_barrier(my_thread, filc_ptr_object(resolved));
}

/* Shared core behind filc_llvm_jit_resolve, the resolve trampoline, and the
   pizlonated FilLLVMJIT_Resolve native: call the user-provided Fil-C resolver
   (a callable taking (name_string, context) and returning a flight pointer),
   root the result, and AddGlobalMapping it into the session so subsequently
   compiled modules bind the symbol directly. Returns the raw address (NULL
   when unresolvable) and, when resolved_out is non-null, the Fil-C pointer. */
static void* filc_llvm_jit_resolve_impl(filc_thread* my_thread,
                                        void* session_handle,
                                        filc_llvm_jit_generated_module_object* module,
                                        const char* name,
                                        filc_ptr* resolved_out)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_ptr resolver;
    filc_ptr resolver_context;
    /* Flight loads with the standard helper. Readers and writers both hold
       api_lock here, and the critical section is a leaf (no exit, no
       allocation), so it neither deadlocks with nor stalls the lock-free
       marker (which never takes this lock). Writers pin before storing (see
       set_current), in lock order api_lock -> global-roots lock, which the
       marker cannot invert since it takes only the latter. */
    pas_lock_lock(&filc_llvm_jit_api_lock);
    resolver = filc_flight_ptr_load_with_manual_tracking(&filc_llvm_jit_resolver_ref);
    resolver_context =
        filc_flight_ptr_load_with_manual_tracking(&filc_llvm_jit_resolver_context_ref);
    pas_lock_unlock(&filc_llvm_jit_api_lock);

    if (resolved_out)
        *resolved_out = filc_ptr_forge_null();
    if (!name || filc_ptr_is_totally_null(resolver))
        return NULL;

    /* The resolver is a user callable; invoke it entered with no locks held.
       filc_call_user_* panics if it throws (resolvers must not throw). */
    filc_ptr name_string = filc_strdup(my_thread, name);
    extern filc_ptr filc_call_user_ptr_ptr_ptr(filc_thread* my_thread, filc_ptr target,
                                               filc_ptr arg0, filc_ptr arg1);
    filc_ptr resolved = filc_call_user_ptr_ptr_ptr(my_thread, resolver, name_string,
                                                   resolver_context);
    if (filc_ptr_is_totally_null(resolved))
        return NULL;
    void* address = filc_ptr_ptr(resolved);

    filc_llvm_jit_record_root(my_thread, module, resolved);

    if (session_handle && address && api->AddGlobalMapping) {
        filc_exit(my_thread);
        api->AddGlobalMapping((FilLLVMJITRef)session_handle, name, address);
        filc_enter(my_thread);
    }

    if (resolved_out)
        *resolved_out = resolved;
    return address;
}

/* Raw-C entry points. These are what the runtime binds (via AddGlobalMapping)
   to the blessed names filc_llvm_jit_resolve, filc_llvm_jit_resolve_raw_pointer,
   and zexternptr when a JIT session is created, shadowing the shim's
   non-root-recording helpers: JITed lazy getters call these, and every
   address handed to JITed code is pinned in the current generated module. */

void* filc_llvm_jit_resolve(const char* name)
{
    filc_thread* my_thread = filc_get_my_thread();
    void* session_handle = NULL;
    filc_llvm_jit_generated_module_object* module = NULL;
    pas_lock_lock(&filc_llvm_jit_api_lock);
    {
        filc_ptr session_ref = filc_flight_ptr_load_with_manual_tracking(
            &filc_llvm_jit_current_session_ref);
        if (!filc_ptr_is_totally_null(session_ref)) {
            filc_llvm_jit_session_object* session =
                (filc_llvm_jit_session_object*)filc_ptr_ptr(session_ref);
            session_handle = session->handle;
        }
    }
    {
        filc_ptr module_ref = filc_flight_ptr_load_with_manual_tracking(
            &filc_llvm_jit_current_module_ref);
        if (!filc_ptr_is_totally_null(module_ref)) {
            module = (filc_llvm_jit_generated_module_object*)
                filc_ptr_ptr(module_ref);
        }
    }
    pas_lock_unlock(&filc_llvm_jit_api_lock);
    return filc_llvm_jit_resolve_impl(my_thread, session_handle, module, name, NULL);
}

/* Trampoline installed as the shim's resolve callback by
   FilLLVMJIT_SetResolveCallback: bridges the raw
   void*(*)(const char*, void*) callback ABI to the Fil-C resolver. Runs on
   whatever Fil-C thread executes JITed code (always entered). */
static void* filc_llvm_jit_c_resolve_trampoline(const char* name, void* context)
{
    PAS_UNUSED_PARAM(context);
    filc_thread* my_thread = filc_get_my_thread();
    void* session_handle = NULL;
    filc_llvm_jit_generated_module_object* module = NULL;
    pas_lock_lock(&filc_llvm_jit_api_lock);
    {
        filc_ptr session_ref = filc_flight_ptr_load_with_manual_tracking(
            &filc_llvm_jit_current_session_ref);
        if (!filc_ptr_is_totally_null(session_ref)) {
            filc_llvm_jit_session_object* session =
                (filc_llvm_jit_session_object*)filc_ptr_ptr(session_ref);
            session_handle = session->handle;
        }
    }
    {
        filc_ptr module_ref = filc_flight_ptr_load_with_manual_tracking(
            &filc_llvm_jit_current_module_ref);
        if (!filc_ptr_is_totally_null(module_ref)) {
            module = (filc_llvm_jit_generated_module_object*)
                filc_ptr_ptr(module_ref);
        }
    }
    pas_lock_unlock(&filc_llvm_jit_api_lock);
    return filc_llvm_jit_resolve_impl(my_thread, session_handle, module, name, NULL);
}

static void* filc_llvm_jit_resolve_raw_impl(filc_thread* my_thread,
                                            void* session_handle,
                                            filc_llvm_jit_generated_module_object* module,
                                            filc_exact_ptr_table* table,
                                            unsigned long long encoded,
                                            filc_ptr* resolved_out)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    if (resolved_out)
        *resolved_out = filc_ptr_forge_null();
    filc_ptr resolved;
    if (table) {
        resolved = filc_exact_ptr_table_decode(my_thread, table, (uintptr_t)encoded);
    } else if (api->ResolveRawPointer) {
        filc_exit(my_thread);
        void* address = api->ResolveRawPointer(encoded);
        filc_enter(my_thread);
        if (!address)
            return NULL;
        resolved = filc_ptr_forge_invalid(address);
    } else
        return NULL;
    if (filc_ptr_is_totally_null(resolved))
        return NULL;
    void* address = filc_ptr_ptr(resolved);
    filc_llvm_jit_record_root(my_thread, module, resolved);
    /* NOTE: unlike the by-name path, there is no symbol to pre-bind here:
       "zexternptr" names the resolver *function* (mapped at session
       creation), not the resolved datum, and per-id caching already lives
       in the JITed code. Overwriting that function mapping with a data
       address would corrupt later modules, so no AddGlobalMapping is done.
       (Reads session_handle/api to keep -Wunused warnings honest.) */
    (void)session_handle;
    (void)api;
    if (resolved_out)
        *resolved_out = resolved;
    return address;
}

void* filc_llvm_jit_resolve_raw_pointer(unsigned long long encoded)
{
    filc_thread* my_thread = filc_get_my_thread();
    void* session_handle = NULL;
    filc_llvm_jit_generated_module_object* module = NULL;
    filc_exact_ptr_table* table = NULL;
    pas_lock_lock(&filc_llvm_jit_api_lock);
    {
        filc_ptr session_ref = filc_flight_ptr_load_with_manual_tracking(
            &filc_llvm_jit_current_session_ref);
        if (!filc_ptr_is_totally_null(session_ref)) {
            filc_llvm_jit_session_object* session =
                (filc_llvm_jit_session_object*)filc_ptr_ptr(session_ref);
            session_handle = session->handle;
        }
    }
    {
        filc_ptr module_ref = filc_flight_ptr_load_with_manual_tracking(
            &filc_llvm_jit_current_module_ref);
        if (!filc_ptr_is_totally_null(module_ref)) {
            module = (filc_llvm_jit_generated_module_object*)
                filc_ptr_ptr(module_ref);
        }
    }
    {
        filc_ptr zexact_ref = filc_flight_ptr_load_with_manual_tracking(
            &filc_llvm_jit_zexact_table_ref);
        if (!filc_ptr_is_totally_null(zexact_ref)) {
            filc_check_access_special(zexact_ref,
                                      FILC_SPECIAL_TYPE_EXACT_PTR_TABLE);
            table = (filc_exact_ptr_table*)filc_ptr_ptr(zexact_ref);
        }
    }
    pas_lock_unlock(&filc_llvm_jit_api_lock);
    return filc_llvm_jit_resolve_raw_impl(my_thread, session_handle, module, table,
                                          encoded, NULL);
}

void* zexternptr(unsigned long long encoded)
{
    return filc_llvm_jit_resolve_raw_pointer(encoded);
}

/* ------------------------------------------------------------------ */
/* Hand-written natives: session API, resolve, and special cases. */

/* NOTE: there is no explicit init native (API v4): the first
   ensure_api (dlopen + GetHeapMemory lazy boot) and the shim's own
   ensure-initialized path inside Create initialize automatically. */

filc_ptr filc_native_FilLLVMJIT_GetLastError(filc_thread* my_thread)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    const char* error = api->GetLastError ? api->GetLastError() : "";
    if (!error)
        error = "";
    return filc_strdup(my_thread, error);
}

filc_ptr filc_native_FilLLVMJIT_Create(filc_thread* my_thread, filc_ptr triple_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    char* triple = filc_check_and_get_tmp_str_or_null(my_thread, triple_ptr);
    filc_ptr result;
    filc_object* object;
    filc_llvm_jit_session_object* session;

    pas_lock_lock(&filc_llvm_jit_api_lock);
    filc_exit(my_thread);
    FilLLVMJITRef handle = api->Create(triple);
    filc_enter(my_thread);
    pas_lock_unlock(&filc_llvm_jit_api_lock);
    if (!handle)
        return filc_ptr_forge_null();

    object = filc_allocate_special_with_heap(
        my_thread, sizeof(filc_llvm_jit_session_object), 16,
        FILC_SPECIAL_TYPE_LLVM_JIT, fugc_destructor_heap);
    session = (filc_llvm_jit_session_object*)filc_object_lower(object);
    session->kind = FILC_LLVM_JIT_KIND_JIT_SESSION;
    pas_lock_construct(&session->lock);
    session->handle = (void*)handle;
    session->current_module = NULL;
    session->current_module_ref = filc_ptr_forge_null();
    result = filc_llvm_jit_publish(my_thread, object);

    /* Pin BEFORE the exited AddGlobalMapping calls
       below. The freshly published session is otherwise kept alive only by
       the caller's native-frame tracking, which an exited thread does not
       contribute as a mark root -- a collection racing the exit window could
       sweep it. Pinning first makes the session immortal up front, so the
       exits below are safe. */
    filc_llvm_jit_pin_global(my_thread, result);

    /* Shadow the shim's non-root-recording helpers with the root-recording
       implementations, so JITed lazy getters pin everything they resolve.
       The handle is copied under the lock and used unlocked: sessions are
       immortal (pinned in the process-global roots below) and their LLVM
       handles are never disposed, so no lock needs to be held across the
       exited calls. */
    void* session_handle;
    pas_lock_lock(&session->lock);
    session_handle = session->handle;
    pas_lock_unlock(&session->lock);
    filc_exit(my_thread);
    api->AddGlobalMapping((FilLLVMJITRef)session_handle,
                          "filc_llvm_jit_resolve", (void*)filc_llvm_jit_resolve);
    api->AddGlobalMapping((FilLLVMJITRef)session_handle,
                          "filc_llvm_jit_resolve_raw_pointer",
                          (void*)filc_llvm_jit_resolve_raw_pointer);
    api->AddGlobalMapping((FilLLVMJITRef)session_handle,
                          "zexternptr", (void*)zexternptr);
    filc_enter(my_thread);

    filc_llvm_jit_set_current(my_thread, result, filc_ptr_forge_null());
    return result;
}

int filc_native_FilLLVMJIT_SetOption(filc_thread* my_thread, filc_ptr jit_ptr,
                                           filc_ptr key_ptr, filc_ptr value_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_session_object* session = filc_llvm_jit_unwrap_session(my_thread, jit_ptr);
    char* key = filc_check_and_get_tmp_str(my_thread, key_ptr);
    char* value = filc_check_and_get_tmp_str(my_thread, value_ptr);
    int result;
    /* Copy the handle under the lock, use it unlocked (see Create: the
       session is immortal, the handle never disposed). */
    void* session_handle;
    pas_lock_lock(&session->lock);
    session_handle = session->handle;
    pas_lock_unlock(&session->lock);
    filc_exit(my_thread);
    result = api->SetOption((FilLLVMJITRef)session_handle, key, value);
    filc_enter(my_thread);
    return result;
}

void filc_native_FilLLVMJIT_SetResolveCallback(filc_thread* my_thread, filc_ptr jit_ptr,
                                                     filc_ptr resolver_ptr, filc_ptr context_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_session_object* session = filc_llvm_jit_unwrap_session(my_thread, jit_ptr);
    FILC_CHECK(!filc_ptr_is_totally_null(resolver_ptr), NULL,
               "FilLLVMJIT_SetResolveCallback: null resolver.");
    filc_check_function_call(resolver_ptr);
    pas_lock_lock(&filc_llvm_jit_api_lock);
    /* Pin before publishing (see set_current). */
    filc_llvm_jit_pin_global(my_thread, resolver_ptr);
    filc_llvm_jit_pin_global(my_thread, context_ptr);
    filc_llvm_jit_resolver_ref = resolver_ptr;
    filc_llvm_jit_resolver_context_ref = context_ptr;
    pas_lock_unlock(&filc_llvm_jit_api_lock);

    pas_lock_lock(&session->lock);
    void* session_handle = session->handle;
    pas_lock_unlock(&session->lock);
    filc_exit(my_thread);
    api->SetResolveCallback((FilLLVMJITRef)session_handle,
                            filc_llvm_jit_c_resolve_trampoline, NULL);
    filc_enter(my_thread);

    filc_llvm_jit_set_current(my_thread,
                              filc_ptr_create_with_special_object_and_manual_tracking(
                                  filc_object_for_special_payload(session)),
                              filc_ptr_forge_null());
}

int filc_native_FilLLVMJIT_SetZExactPointerTable(filc_thread* my_thread, filc_ptr jit_ptr,
                                                       filc_ptr table_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_session_object* session = filc_llvm_jit_unwrap_session(my_thread, jit_ptr);
    FILC_CHECK(!filc_ptr_is_totally_null(table_ptr), NULL,
               "FilLLVMJIT_SetZExactPointerTable: null table.");
    filc_check_access_special(table_ptr, FILC_SPECIAL_TYPE_EXACT_PTR_TABLE);
    void* table = filc_ptr_ptr(table_ptr);
    int result;
    void* session_handle;
    pas_lock_lock(&filc_llvm_jit_api_lock);
    /* Pin before publishing (see set_current). */
    filc_llvm_jit_pin_global(my_thread, table_ptr);
    filc_llvm_jit_zexact_table_ref = table_ptr;
    pas_lock_unlock(&filc_llvm_jit_api_lock);
    pas_lock_lock(&session->lock);
    session_handle = session->handle;
    pas_lock_unlock(&session->lock);
    filc_exit(my_thread);
    result = api->SetZExactPointerTable((FilLLVMJITRef)session_handle, table);
    filc_enter(my_thread);
    return result;
}

int filc_native_FilLLVMJIT_AddModule(filc_thread* my_thread, filc_ptr jit_ptr,
                                           filc_ptr module_ptr, filc_ptr name_ptr,
                                           filc_ptr out_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_session_object* session = filc_llvm_jit_unwrap_session(my_thread, jit_ptr);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module_as_donor = NULL;
    filc_llvm_jit_object* module_obj =
        filc_llvm_jit_unwrap_obj(my_thread, module_ptr, FILC_LLVM_JIT_KIND_MODULE,
                                 &context, &module_as_donor);
    FILC_CHECK(module_obj, NULL, "FilLLVMJIT_AddModule: null module.");
    filc_check_write(out_ptr, sizeof(filc_ptr));
    char* name = filc_check_and_get_tmp_str_or_null(my_thread, name_ptr);
    FilLLVMJITModuleRef out = NULL;
    int status;
    /* Locking: the session lock is NOT held across the compile call below.
       The old code held it across filc_exit + the full JIT compile +
       allocation, which self-deadlocked when the allocation triggered a
       collection (the collector blocked on the lock this thread held while
       this thread waited for the collector) and stalled every mark for the
       duration of the compile. The context lock alone serializes the LLVM
       call (LLVM contexts are not thread-safe); the shim serializes the
       session side internally. Handles are stable without the session lock:
       sessions are immortal (pinned in the process-global roots) and their
       LLVM handles are never disposed. Only the publish step at the end
       takes the session lock, and it performs no exit and no allocation. */
    void* session_handle;
    void* module_handle = module_obj->handle;
    filc_llvm_jit_context_state* context_state = context->state;
    pas_lock_lock(&session->lock);
    session_handle = session->handle;
    pas_lock_unlock(&session->lock);
    pas_lock_lock(&context_state->lock);
    filc_exit(my_thread);
    status = api->AddModule((FilLLVMJITRef)session_handle,
                            (LLVMModuleRef)module_handle, name, &out);
    filc_enter(my_thread);
    pas_lock_unlock(&context_state->lock);
    if (status || !out) {
        filc_store_ptr(my_thread, out_ptr, 0, filc_ptr_forge_null());
        return -1;
    }
    filc_object* object = filc_allocate_special_with_heap(
        my_thread, sizeof(filc_llvm_jit_generated_module_object), 16,
        FILC_SPECIAL_TYPE_LLVM_JIT, fugc_destructor_heap);
    filc_llvm_jit_generated_module_object* generated =
        (filc_llvm_jit_generated_module_object*)filc_object_lower(object);
    generated->kind = FILC_LLVM_JIT_KIND_GENERATED_MODULE;
    pas_lock_construct(&generated->lock);
    generated->handle = (void*)out;
    generated->session_handle = session_handle;
    generated->roots = NULL;
    generated->num_roots = 0;
    generated->roots_capacity = 0;
    generated->roots_generation = 0;
    generated->retired_roots = NULL;
    generated->num_retired = 0;
    generated->retired_capacity = 0;
    filc_ptr result = filc_llvm_jit_publish(my_thread, object);
    /* Track before publishing so the keep-alive is rooted even if a
       collection sneaks in between (tracking may allocate, hence no lock
       held here). */
    filc_thread_track_object(my_thread, filc_ptr_object(result));
    /* Pin BEFORE the lock-free publish below.
       The marker loads current_module_ref without holding session->lock
       (via filc_flight_ptr_load_with_manual_tracking), so it must never
       observe a live-but-unpinned module. (set_current below pins again;
       duplicate pins are idempotent -- the array just marks the same object
       twice.) */
    filc_llvm_jit_pin_global(my_thread, result);
    pas_lock_lock(&session->lock);
    session->current_module = generated;
    /* Plain flight store under the session lock; the marker loads it with
       the standard helper. Pin-before-publish plus the Steele barrier below
       cover any racing scan (see filc_llvm_jit_mark_outgoing_ptrs). */
    session->current_module_ref = result;
    pas_lock_unlock(&session->lock);
    /* Insertion barrier for a concurrently marking
       collector (mirrors record_root). Steele-style: mark the newly
       published module itself, so the same cycle keeps it alive no matter
       when the session's own scan ran. Barrierring the session would be a
       no-op (it is immortal, hence already marked, and barrier_impl does not
       re-push marked objects). Without this, a white module published after
       the session scan -- and after the global scan and its publisher's
       thread handshake -- could be missed and swept while still current;
       forced collections racing AddModule caught verify reporting exactly
       that. */
    filc_store_barrier(my_thread, filc_ptr_object(result));

    filc_llvm_jit_set_current(my_thread,
                              filc_ptr_create_with_special_object_and_manual_tracking(
                                  filc_object_for_special_payload(session)),
                              result);
    filc_store_ptr(my_thread, out_ptr, 0, result);
    return 0;
}

int filc_native_FilLLVMJIT_AddGlobalMapping(filc_thread* my_thread, filc_ptr jit_ptr,
                                                  filc_ptr name_ptr, filc_ptr address_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_session_object* session = filc_llvm_jit_unwrap_session(my_thread, jit_ptr);
    char* name = filc_check_and_get_tmp_str(my_thread, name_ptr);
    void* address;
    if (filc_ptr_is_boxed_int(address_ptr)) {
        /* Capability-less (forged) pointer: a raw host address, e.g. a dlsym
           result or a token previously returned by GetFunctionAddress (those
           come back forged). Genuine host code/data addresses never live in
           the first page, so reject null and small-integer garbage that would
           otherwise be installed as a JIT mapping and fault (or worse) when
           JITed code reaches it. */
        address = filc_ptr_ptr(address_ptr);
        FILC_CHECK((uintptr_t)address >= 4096, NULL,
                   "FilLLVMJIT_AddGlobalMapping: refusing to map '%s' to forged "
                   "pointer %p, which is not a plausible host address (small "
                   "integers and null cannot be host code/data addresses; pass "
                   "a real function or object address).",
                   name, address);
    } else {
        /* Genuine Fil-C capability (e.g. (void*)host_add or (void*)&mydata):
           the address must lie within the allocation the capability describes.
           zmkptr-derived pointers keep their original capability while
           retargeting the address, so an out-of-bounds address here is a
           forged capability that would hand JITed code an unowned address.

           Exception: genuine Fil-C function payloads. Special objects
           (notably functions, FILC_SPECIAL_TYPE_FUNCTION) have null size:
           lower/upper describe only the one-word object header (upper ==
           lower) while the genuine payload address lives in the aux pointer
           -- the call path (checkFunctionAndGetLower) requires ptr == aux
           for functions. A zmkptr retarget to an unrelated address still
           fails below; only the exact aux payload of a function object is
           additionally accepted. */
        void* lower = filc_ptr_lower(address_ptr);
        void* upper = filc_ptr_upper(address_ptr);
        address = filc_ptr_ptr(address_ptr);
        bool in_bounds = (address >= lower && address <= upper);
        if (!in_bounds) {
            filc_object* object = filc_ptr_object(address_ptr);
            in_bounds = (filc_object_is_special(object) &&
                         filc_object_special_type(object) == FILC_SPECIAL_TYPE_FUNCTION &&
                         address == (void*)filc_object_aux_ptr(object));
        }
        FILC_CHECK(in_bounds, NULL,
                   "FilLLVMJIT_AddGlobalMapping: refusing to map '%s' to address "
                   "%p, which is outside its capability [%p, %p] (and is not the "
                   "function payload of a Fil-C function capability; a zmkptr-derived "
                   "or otherwise forged Fil-C capability would confuse JITed code).",
                   name, address, lower, upper);
    }
    int result;
    void* session_handle;
    pas_lock_lock(&session->lock);
    session_handle = session->handle;
    pas_lock_unlock(&session->lock);
    filc_exit(my_thread);
    result = api->AddGlobalMapping((FilLLVMJITRef)session_handle, name, address);
    filc_enter(my_thread);
    return result;
}

static filc_ptr filc_llvm_jit_lookup_address(filc_thread* my_thread, filc_ptr jit_ptr,
                                             filc_ptr name_ptr,
                                             void* (*lookup)(FilLLVMJITRef, const char*))
{
    filc_llvm_jit_session_object* session = filc_llvm_jit_unwrap_session(my_thread, jit_ptr);
    char* name = filc_check_and_get_tmp_str(my_thread, name_ptr);
    void* address;
    void* session_handle;
    pas_lock_lock(&session->lock);
    session_handle = session->handle;
    pas_lock_unlock(&session->lock);
    filc_exit(my_thread);
    address = lookup((FilLLVMJITRef)session_handle, name);
    filc_enter(my_thread);
    if (!address)
        return filc_ptr_forge_null();
    return filc_ptr_forge_invalid(address);
}

filc_ptr filc_native_FilLLVMJIT_GetFunctionAddress(filc_thread* my_thread, filc_ptr jit_ptr,
                                                        filc_ptr name_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    return filc_llvm_jit_lookup_address(my_thread, jit_ptr, name_ptr,
                                        api->GetFunctionAddress);
}

filc_ptr filc_native_FilLLVMJIT_GetGlobalValueAddress(filc_thread* my_thread,
                                                            filc_ptr jit_ptr, filc_ptr name_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    return filc_llvm_jit_lookup_address(my_thread, jit_ptr, name_ptr,
                                        api->GetGlobalValueAddress);
}

filc_ptr filc_native_FilLLVMJIT_GetPointerToGlobal(filc_thread* my_thread, filc_ptr jit_ptr,
                                                         filc_ptr name_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    return filc_llvm_jit_lookup_address(my_thread, jit_ptr, name_ptr,
                                        api->GetPointerToGlobal);
}

/* Fil-C-visible bridge to the shim's test-only
   FilLLVMJIT_InvokeIntFunction executor (see the generated user header
   for scope and limits).  Runs the JITed no-arg `int name(void)` function
   and stores its result through out_ptr; returns 0 on success, nonzero on
   failure (see FilLLVMJIT_GetLastError).  The version gate below is
   vestigial under API v4 (the runtime requires exactly v4, which always
   carries the tail); it stays as a defensive check. */
int filc_native_FilLLVMJIT_InvokeIntFunction(filc_thread* my_thread, filc_ptr jit_ptr,
                                                   filc_ptr name_ptr, filc_ptr out_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_session_object* session = filc_llvm_jit_unwrap_session(my_thread, jit_ptr);
    char* name = filc_check_and_get_tmp_str(my_thread, name_ptr);
    int result;
    int value = 0;
    void* session_handle;
    FILC_CHECK(api->version >= 2 && api->InvokeIntFunction != NULL, NULL,
               "FilLLVMJIT_InvokeIntFunction requires shim API version >= 2 (loaded shim "
               "reports version %llu).",
               api->version);
    filc_check_write(out_ptr, sizeof(int));
    pas_lock_lock(&session->lock);
    session_handle = session->handle;
    pas_lock_unlock(&session->lock);
    filc_exit(my_thread);
    result = api->InvokeIntFunction((FilLLVMJITRef)session_handle, name, &value);
    filc_enter(my_thread);
    if (!result)
        *(int*)filc_ptr_ptr(out_ptr) = value;
    return result;
}

filc_ptr filc_native_FilLLVMJIT_Resolve(filc_thread* my_thread, filc_ptr module_ptr,
                                       filc_ptr name_ptr)
{
    filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_generated_module_object* module =
        filc_llvm_jit_unwrap_generated_module(my_thread, module_ptr);
    char* name = filc_check_and_get_tmp_str_or_null(my_thread, name_ptr);
    void* session_handle = NULL;
    filc_llvm_jit_generated_module_object* target = module;
    if (module) {
        session_handle = module->session_handle;
    } else {
        pas_lock_lock(&filc_llvm_jit_api_lock);
        {
            filc_ptr session_ref = filc_flight_ptr_load_with_manual_tracking(
                &filc_llvm_jit_current_session_ref);
            if (!filc_ptr_is_totally_null(session_ref)) {
                filc_llvm_jit_session_object* session =
                    (filc_llvm_jit_session_object*)filc_ptr_ptr(session_ref);
                session_handle = session->handle;
            }
        }
        {
            filc_ptr module_ref = filc_flight_ptr_load_with_manual_tracking(
                &filc_llvm_jit_current_module_ref);
            if (!filc_ptr_is_totally_null(module_ref)) {
                target = (filc_llvm_jit_generated_module_object*)
                    filc_ptr_ptr(module_ref);
            }
        }
        pas_lock_unlock(&filc_llvm_jit_api_lock);
    }
    filc_ptr resolved = filc_ptr_forge_null();
    filc_llvm_jit_resolve_impl(my_thread, session_handle, target, name, &resolved);
    return resolved;
}

filc_ptr filc_native_FilLLVMJIT_ResolveRawPointer(filc_thread* my_thread, filc_ptr module_ptr,
                                                   filc_ptr table_ptr,
                                                   unsigned long long encoded)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_generated_module_object* module =
        filc_llvm_jit_unwrap_generated_module(my_thread, module_ptr);
    filc_exact_ptr_table* table = NULL;
    if (!filc_ptr_is_totally_null(table_ptr)) {
        filc_check_access_special(table_ptr, FILC_SPECIAL_TYPE_EXACT_PTR_TABLE);
        table = (filc_exact_ptr_table*)filc_ptr_ptr(table_ptr);
        pas_lock_lock(&filc_llvm_jit_api_lock);
        /* Pin before publishing (see set_current). */
        filc_llvm_jit_pin_global(my_thread, table_ptr);
        filc_llvm_jit_zexact_table_ref = table_ptr;
        pas_lock_unlock(&filc_llvm_jit_api_lock);
    }
    void* session_handle = NULL;
    filc_llvm_jit_generated_module_object* target = module;
    if (module) {
        session_handle = module->session_handle;
    } else {
        pas_lock_lock(&filc_llvm_jit_api_lock);
        {
            filc_ptr session_ref = filc_flight_ptr_load_with_manual_tracking(
                &filc_llvm_jit_current_session_ref);
            if (!filc_ptr_is_totally_null(session_ref)) {
                filc_llvm_jit_session_object* session =
                    (filc_llvm_jit_session_object*)filc_ptr_ptr(session_ref);
                session_handle = session->handle;
            }
        }
        {
            filc_ptr module_ref = filc_flight_ptr_load_with_manual_tracking(
                &filc_llvm_jit_current_module_ref);
            if (!filc_ptr_is_totally_null(module_ref)) {
                target = (filc_llvm_jit_generated_module_object*)
                    filc_ptr_ptr(module_ref);
            }
        }
        pas_lock_unlock(&filc_llvm_jit_api_lock);
    }
    if (table && session_handle && api->SetZExactPointerTable) {
        filc_exit(my_thread);
        api->SetZExactPointerTable((FilLLVMJITRef)session_handle, (void*)table);
        filc_enter(my_thread);
    }
    filc_ptr resolved = filc_ptr_forge_null();
    filc_llvm_jit_resolve_raw_impl(my_thread, session_handle, target, table, encoded,
                                   &resolved);
    return resolved;
}

/* ------------------------------------------------------------------ */
/* Hand-written natives for functions that the mechanical generator
   cannot express: out-arrays (with companion count queries), out-scalars,
   raw-word arrays, and multi-array calls. */

void filc_native_LLVMGetVersion(filc_thread* my_thread, filc_ptr major_ptr,
                                      filc_ptr minor_ptr, filc_ptr patch_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    unsigned major = 0;
    unsigned minor = 0;
    unsigned patch = 0;
    filc_check_write(major_ptr, sizeof(unsigned));
    filc_check_write(minor_ptr, sizeof(unsigned));
    filc_check_write(patch_ptr, sizeof(unsigned));
    filc_exit(my_thread);
    api->LLVMGetVersion(&major, &minor, &patch);
    filc_enter(my_thread);
    *(unsigned*)filc_ptr_ptr(major_ptr) = major;
    *(unsigned*)filc_ptr_ptr(minor_ptr) = minor;
    *(unsigned*)filc_ptr_ptr(patch_ptr) = patch;
}

double filc_native_LLVMConstRealGetDouble(filc_thread* my_thread, filc_ptr value_ptr,
                                                filc_ptr loses_info_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* value =
        filc_llvm_jit_unwrap_obj(my_thread, value_ptr, FILC_LLVM_JIT_KIND_VALUE,
                                 &context, &module);
    FILC_CHECK(value, NULL, "LLVMConstRealGetDouble: null value.");
    int loses_info = 0;
    double result;
    filc_check_write(loses_info_ptr, sizeof(int));
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    result = api->LLVMConstRealGetDouble((LLVMValueRef)value->handle, &loses_info);
    filc_enter(my_thread);
    pas_lock_unlock(&context->state->lock);
    *(int*)filc_ptr_ptr(loses_info_ptr) = loses_info;
    return result;
}

filc_ptr filc_native_LLVMGlobalCopyAllMetadata(filc_thread* my_thread, filc_ptr value_ptr,
                                                     filc_ptr num_entries_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* value =
        filc_llvm_jit_unwrap_obj(my_thread, value_ptr, FILC_LLVM_JIT_KIND_VALUE,
                                 &context, &module);
    FILC_CHECK(value, NULL, "LLVMGlobalCopyAllMetadata: null value.");
    size_t num_entries = 0;
    LLVMValueMetadataEntry* entries = NULL;
    filc_check_write(num_entries_ptr, sizeof(size_t));
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    entries = api->LLVMGlobalCopyAllMetadata((LLVMValueRef)value->handle, &num_entries);
    filc_enter(my_thread);
    /* The entries array is never freed (API v1 has no Dispose for it); the
       wrapper records the count so the getters can bounds-check. */
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module,
                                         FILC_LLVM_JIT_KIND_METADATA_ENTRIES,
                                         (void*)entries);
    if (!filc_ptr_is_totally_null(result)) {
        ((filc_llvm_jit_entries_object*)filc_ptr_ptr(result))->count = num_entries;
    }
    pas_lock_unlock(&context->state->lock);
    *(size_t*)filc_ptr_ptr(num_entries_ptr) = num_entries;
    return result;
}

filc_ptr filc_native_LLVMModuleFlagEntriesGetKey(filc_thread* my_thread,
                                                       filc_ptr entries_ptr, unsigned index)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* entries =
        filc_llvm_jit_unwrap_obj(my_thread, entries_ptr,
                                 FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES,
                                 &context, &module);
    FILC_CHECK(entries, NULL, "LLVMModuleFlagEntriesGetKey: null entries.");
    FILC_CHECK(context, NULL, "LLVMModuleFlagEntriesGetKey: entries without context.");
    const char* key;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    /* Len is redundant: Fil-C strings know their own length. */
    key = api->LLVMModuleFlagEntriesGetKey((LLVMModuleFlagEntry*)entries->handle, index,
                                           NULL);
    filc_enter(my_thread);
    filc_ptr result = key ? filc_strdup(my_thread, key) : filc_ptr_forge_null();
    pas_lock_unlock(&context->state->lock);
    return result;
}

filc_ptr filc_native_LLVMTargetExtTypeInContext(
    filc_thread* my_thread, filc_ptr context_ptr, filc_ptr name_ptr,
    filc_ptr type_params_ptr, unsigned type_param_count,
    filc_ptr int_params_ptr, unsigned int_param_count)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = filc_llvm_jit_unwrap_context(my_thread, context_ptr);
    filc_llvm_jit_object* module = NULL;
    char* name = filc_check_and_get_tmp_str(my_thread, name_ptr);
    LLVMTypeRef* type_params = NULL;
    unsigned* int_params = NULL;
    if (!filc_ptr_is_totally_null(type_params_ptr)) {
        void** decoded = filc_llvm_jit_decode_handle_array(
            my_thread, type_params_ptr, type_param_count, FILC_LLVM_JIT_KIND_TYPE,
            &context, &module);
        type_params = (LLVMTypeRef*)decoded;
    } else {
        FILC_CHECK(!type_param_count, NULL,
                   "LLVMTargetExtTypeInContext: null type params with nonzero count.");
    }
    if (!filc_ptr_is_totally_null(int_params_ptr)) {
        filc_check_read(int_params_ptr, filc_mul_size(int_param_count, sizeof(unsigned)));
        int_params = (unsigned*)filc_ptr_ptr(int_params_ptr);
    } else {
        FILC_CHECK(!int_param_count, NULL,
                   "LLVMTargetExtTypeInContext: null int params with nonzero count.");
    }
    LLVMTypeRef result_raw;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMTargetExtTypeInContext((LLVMContextRef)context->state->handle, name,
                                                 type_params, type_param_count,
                                                 int_params, int_param_count);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, NULL,
                                         FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstIntOfArbitraryPrecision(
    filc_thread* my_thread, filc_ptr int_ty_ptr, unsigned num_words, filc_ptr words_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* int_ty =
        filc_llvm_jit_unwrap_obj(my_thread, int_ty_ptr, FILC_LLVM_JIT_KIND_TYPE,
                                 &context, &module);
    FILC_CHECK(int_ty, NULL, "LLVMConstIntOfArbitraryPrecision: null type.");
    const uint64_t* words = NULL;
    if (!filc_ptr_is_totally_null(words_ptr)) {
        filc_check_read(words_ptr, filc_mul_size(num_words, sizeof(uint64_t)));
        words = (const uint64_t*)filc_ptr_ptr(words_ptr);
    } else {
        FILC_CHECK(!num_words, NULL,
                   "LLVMConstIntOfArbitraryPrecision: null words with nonzero count.");
    }
    LLVMValueRef result_raw;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstIntOfArbitraryPrecision((LLVMTypeRef)int_ty->handle, num_words,
                                                       words);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module,
                                         FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->state->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateConstantRangeAttribute(
    filc_thread* my_thread, filc_ptr context_ptr, unsigned kind_id, unsigned num_bits,
    filc_ptr lower_words_ptr, filc_ptr upper_words_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = filc_llvm_jit_unwrap_context(my_thread, context_ptr);
    /* The C API derives the word count from the bit width. */
    unsigned num_words = (num_bits + 63u) / 64u;
    const uint64_t* lower_words = NULL;
    const uint64_t* upper_words = NULL;
    if (!filc_ptr_is_totally_null(lower_words_ptr)) {
        filc_check_read(lower_words_ptr, filc_mul_size(num_words, sizeof(uint64_t)));
        lower_words = (const uint64_t*)filc_ptr_ptr(lower_words_ptr);
    } else {
        FILC_CHECK(!num_words, NULL,
                   "LLVMCreateConstantRangeAttribute: null lower words with nonzero count.");
    }
    if (!filc_ptr_is_totally_null(upper_words_ptr)) {
        filc_check_read(upper_words_ptr, filc_mul_size(num_words, sizeof(uint64_t)));
        upper_words = (const uint64_t*)filc_ptr_ptr(upper_words_ptr);
    } else {
        FILC_CHECK(!num_words, NULL,
                   "LLVMCreateConstantRangeAttribute: null upper words with nonzero count.");
    }
    LLVMAttributeRef result_raw;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCreateConstantRangeAttribute((LLVMContextRef)context->state->handle,
                                                       kind_id, num_bits,
                                                       lower_words, upper_words);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, NULL,
                                         FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    pas_lock_unlock(&context->state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCallBr(
    filc_thread* my_thread, filc_ptr builder_ptr, filc_ptr ty_ptr, filc_ptr fn_ptr,
    filc_ptr default_dest_ptr, filc_ptr indirect_dests_ptr, unsigned num_indirect_dests,
    filc_ptr args_ptr, unsigned num_args, filc_ptr bundles_ptr, unsigned num_bundles,
    filc_ptr name_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    /* Operand bundles have no constructors in API v1, so only null is
       accepted today (the count must be zero with it). */
    FILC_CHECK(filc_ptr_is_totally_null(bundles_ptr), NULL,
               "LLVMBuildCallBr: operand bundles are not supported in API v1 (pass null).");
    FILC_CHECK(!num_bundles, NULL,
               "LLVMBuildCallBr: nonzero bundle count with null bundles.");
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* builder =
        filc_llvm_jit_unwrap_obj(my_thread, builder_ptr, FILC_LLVM_JIT_KIND_BUILDER,
                                 &context, &module);
    FILC_CHECK(builder, NULL, "LLVMBuildCallBr: null builder.");
    filc_llvm_jit_object* ty =
        filc_llvm_jit_unwrap_obj(my_thread, ty_ptr, FILC_LLVM_JIT_KIND_TYPE,
                                 &context, &module);
    filc_llvm_jit_object* fn =
        filc_llvm_jit_unwrap_obj(my_thread, fn_ptr, FILC_LLVM_JIT_KIND_VALUE,
                                 &context, &module);
    filc_llvm_jit_object* default_dest =
        filc_llvm_jit_unwrap_obj(my_thread, default_dest_ptr, FILC_LLVM_JIT_KIND_BASIC_BLOCK,
                                 &context, &module);
    LLVMBasicBlockRef* indirect_dests = (LLVMBasicBlockRef*)filc_llvm_jit_decode_handle_array(
        my_thread, indirect_dests_ptr, num_indirect_dests, FILC_LLVM_JIT_KIND_BASIC_BLOCK,
        &context, &module);
    LLVMValueRef* args = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(
        my_thread, args_ptr, num_args, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* name = filc_check_and_get_tmp_str_or_null(my_thread, name_ptr);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCallBr((LLVMBuilderRef)builder->handle,
                                      (LLVMTypeRef)filc_llvm_jit_handle_of(ty),
                                      (LLVMValueRef)filc_llvm_jit_handle_of(fn),
                                      (LLVMBasicBlockRef)filc_llvm_jit_handle_of(default_dest),
                                      indirect_dests, num_indirect_dests,
                                      args, num_args, NULL, 0, name);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module,
                                         FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (module)
        filc_llvm_jit_builder_adopt_module(my_thread, builder, module);
    pas_lock_unlock(&context->state->lock);
    return result;
}

/* Out-array pattern shared by the LLVMGet* family: the caller queries the
   companion count and the getter into a host tmp buffer (under the context
   lock, exited), then this emits wrappers into the caller's preallocated
   Fil-C pointer array with checked stores (entered). Under-allocation
   panics cleanly instead of corrupting memory. */
static void filc_llvm_jit_emit_out_array(filc_thread* my_thread,
                                         filc_llvm_jit_context_object* context,
                                         filc_llvm_jit_object* module,
                                         void** raws, unsigned count,
                                         filc_ptr array_ptr, filc_llvm_jit_kind kind)
{
    unsigned index;
    for (index = 0; index < count; ++index) {
        filc_ptr wrapped = filc_llvm_jit_wrap(my_thread, context, module, kind,
                                              raws[index]);
        filc_store_ptr(my_thread, array_ptr,
                       (ptrdiff_t)filc_mul_size(index, sizeof(void*)), wrapped);
    }
}


void filc_native_LLVMGetAttributesAtIndex(filc_thread* my_thread, filc_ptr fn_ptr,
                                                unsigned index, filc_ptr attrs_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* fn = filc_llvm_jit_unwrap_obj(
        my_thread, fn_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    FILC_CHECK(fn, NULL, "LLVMGetAttributesAtIndex: null function.");
    LLVMAttributeRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    count = api->LLVMGetAttributeCountAtIndex((LLVMValueRef)fn->handle, index);
    if (count)
        tmp = (LLVMAttributeRef*)malloc((size_t)count * sizeof(LLVMAttributeRef));
    if (!count || tmp)
        api->LLVMGetAttributesAtIndex((LLVMValueRef)fn->handle, index, tmp);
    filc_enter(my_thread);
    if (count && !tmp) {
        pas_lock_unlock(&context->state->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetAttributesAtIndex.");
    }
    filc_llvm_jit_emit_out_array(my_thread, context, module, (void**)tmp, count, attrs_ptr,
                                 FILC_LLVM_JIT_KIND_ATTRIBUTE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetCallSiteAttributes(filc_thread* my_thread, filc_ptr call_ptr,
                                                 unsigned index, filc_ptr attrs_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* call = filc_llvm_jit_unwrap_obj(
        my_thread, call_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    FILC_CHECK(call, NULL, "LLVMGetCallSiteAttributes: null call.");
    LLVMAttributeRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    count = api->LLVMGetCallSiteAttributeCount((LLVMValueRef)call->handle, index);
    if (count)
        tmp = (LLVMAttributeRef*)malloc((size_t)count * sizeof(LLVMAttributeRef));
    if (!count || tmp)
        api->LLVMGetCallSiteAttributes((LLVMValueRef)call->handle, index, tmp);
    filc_enter(my_thread);
    if (count && !tmp) {
        pas_lock_unlock(&context->state->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetCallSiteAttributes.");
    }
    filc_llvm_jit_emit_out_array(my_thread, context, module, (void**)tmp, count, attrs_ptr,
                                 FILC_LLVM_JIT_KIND_ATTRIBUTE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetBasicBlocks(filc_thread* my_thread, filc_ptr fn_ptr,
                                          filc_ptr blocks_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* fn = filc_llvm_jit_unwrap_obj(
        my_thread, fn_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    FILC_CHECK(fn, NULL, "LLVMGetBasicBlocks: null function.");
    LLVMBasicBlockRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    count = api->LLVMCountBasicBlocks((LLVMValueRef)fn->handle);
    if (count)
        tmp = (LLVMBasicBlockRef*)malloc((size_t)count * sizeof(LLVMBasicBlockRef));
    if (!count || tmp)
        api->LLVMGetBasicBlocks((LLVMValueRef)fn->handle, tmp);
    filc_enter(my_thread);
    if (count && !tmp) {
        pas_lock_unlock(&context->state->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetBasicBlocks.");
    }
    filc_llvm_jit_emit_out_array(my_thread, context, module, (void**)tmp, count, blocks_ptr,
                                 FILC_LLVM_JIT_KIND_BASIC_BLOCK);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetHandlers(filc_thread* my_thread, filc_ptr catch_switch_ptr,
                                       filc_ptr handlers_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* catch_switch = filc_llvm_jit_unwrap_obj(
        my_thread, catch_switch_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    FILC_CHECK(catch_switch, NULL, "LLVMGetHandlers: null catchswitch.");
    LLVMBasicBlockRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    count = api->LLVMGetNumHandlers((LLVMValueRef)catch_switch->handle);
    if (count)
        tmp = (LLVMBasicBlockRef*)malloc((size_t)count * sizeof(LLVMBasicBlockRef));
    if (!count || tmp)
        api->LLVMGetHandlers((LLVMValueRef)catch_switch->handle, tmp);
    filc_enter(my_thread);
    if (count && !tmp) {
        pas_lock_unlock(&context->state->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetHandlers.");
    }
    filc_llvm_jit_emit_out_array(my_thread, context, module, (void**)tmp, count, handlers_ptr,
                                 FILC_LLVM_JIT_KIND_BASIC_BLOCK);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetMDNodeOperands(filc_thread* my_thread, filc_ptr value_ptr,
                                             filc_ptr dest_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* value = filc_llvm_jit_unwrap_obj(
        my_thread, value_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    FILC_CHECK(value, NULL, "LLVMGetMDNodeOperands: null value.");
    LLVMValueRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    count = api->LLVMGetMDNodeNumOperands((LLVMValueRef)value->handle);
    if (count)
        tmp = (LLVMValueRef*)malloc((size_t)count * sizeof(LLVMValueRef));
    if (!count || tmp)
        api->LLVMGetMDNodeOperands((LLVMValueRef)value->handle, tmp);
    filc_enter(my_thread);
    if (count && !tmp) {
        pas_lock_unlock(&context->state->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetMDNodeOperands.");
    }
    filc_llvm_jit_emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_VALUE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetNamedMetadataOperands(filc_thread* my_thread, filc_ptr module_ptr,
                                                    filc_ptr name_ptr, filc_ptr dest_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module_donor = NULL;
    filc_llvm_jit_object* module_obj = filc_llvm_jit_unwrap_obj(
        my_thread, module_ptr, FILC_LLVM_JIT_KIND_MODULE, &context, &module_donor);
    FILC_CHECK(module_obj, NULL, "LLVMGetNamedMetadataOperands: null module.");
    char* name = filc_check_and_get_tmp_str(my_thread, name_ptr);
    LLVMValueRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    count = api->LLVMGetNamedMetadataNumOperands((LLVMModuleRef)module_obj->handle, name);
    if (count)
        tmp = (LLVMValueRef*)malloc((size_t)count * sizeof(LLVMValueRef));
    if (!count || tmp)
        api->LLVMGetNamedMetadataOperands((LLVMModuleRef)module_obj->handle, name, tmp);
    filc_enter(my_thread);
    if (count && !tmp) {
        pas_lock_unlock(&context->state->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetNamedMetadataOperands.");
    }
    filc_llvm_jit_emit_out_array(my_thread, context, module_obj, (void**)tmp, count,
                                 dest_ptr, FILC_LLVM_JIT_KIND_VALUE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetParamTypes(filc_thread* my_thread, filc_ptr function_ty_ptr,
                                         filc_ptr dest_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* function_ty = filc_llvm_jit_unwrap_obj(
        my_thread, function_ty_ptr, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    FILC_CHECK(function_ty, NULL, "LLVMGetParamTypes: null type.");
    LLVMTypeRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    count = api->LLVMCountParamTypes((LLVMTypeRef)function_ty->handle);
    if (count)
        tmp = (LLVMTypeRef*)malloc((size_t)count * sizeof(LLVMTypeRef));
    if (!count || tmp)
        api->LLVMGetParamTypes((LLVMTypeRef)function_ty->handle, tmp);
    filc_enter(my_thread);
    if (count && !tmp) {
        pas_lock_unlock(&context->state->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetParamTypes.");
    }
    filc_llvm_jit_emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_TYPE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetParams(filc_thread* my_thread, filc_ptr fn_ptr, filc_ptr dest_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* fn = filc_llvm_jit_unwrap_obj(
        my_thread, fn_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    FILC_CHECK(fn, NULL, "LLVMGetParams: null function.");
    LLVMValueRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    count = api->LLVMCountParams((LLVMValueRef)fn->handle);
    if (count)
        tmp = (LLVMValueRef*)malloc((size_t)count * sizeof(LLVMValueRef));
    if (!count || tmp)
        api->LLVMGetParams((LLVMValueRef)fn->handle, tmp);
    filc_enter(my_thread);
    if (count && !tmp) {
        pas_lock_unlock(&context->state->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetParams.");
    }
    filc_llvm_jit_emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_VALUE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetStructElementTypes(filc_thread* my_thread, filc_ptr struct_ty_ptr,
                                                 filc_ptr dest_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* struct_ty = filc_llvm_jit_unwrap_obj(
        my_thread, struct_ty_ptr, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    FILC_CHECK(struct_ty, NULL, "LLVMGetStructElementTypes: null type.");
    LLVMTypeRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    count = api->LLVMCountStructElementTypes((LLVMTypeRef)struct_ty->handle);
    if (count)
        tmp = (LLVMTypeRef*)malloc((size_t)count * sizeof(LLVMTypeRef));
    if (!count || tmp)
        api->LLVMGetStructElementTypes((LLVMTypeRef)struct_ty->handle, tmp);
    filc_enter(my_thread);
    if (count && !tmp) {
        pas_lock_unlock(&context->state->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetStructElementTypes.");
    }
    filc_llvm_jit_emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_TYPE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetSubtypes(filc_thread* my_thread, filc_ptr ty_ptr, filc_ptr dest_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* ty = filc_llvm_jit_unwrap_obj(
        my_thread, ty_ptr, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    FILC_CHECK(ty, NULL, "LLVMGetSubtypes: null type.");
    LLVMTypeRef* tmp = NULL;
    unsigned count = 0;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    count = api->LLVMGetNumContainedTypes((LLVMTypeRef)ty->handle);
    if (count)
        tmp = (LLVMTypeRef*)malloc((size_t)count * sizeof(LLVMTypeRef));
    if (!count || tmp)
        api->LLVMGetSubtypes((LLVMTypeRef)ty->handle, tmp);
    filc_enter(my_thread);
    if (count && !tmp) {
        pas_lock_unlock(&context->state->lock);
        filc_safety_panic(NULL, "out of memory in LLVMGetSubtypes.");
    }
    filc_llvm_jit_emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_TYPE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

filc_ptr filc_native_LLVMContextCreate(filc_thread* my_thread)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    LLVMContextRef handle;
    filc_exit(my_thread);
    handle = api->LLVMContextCreate();
    filc_enter(my_thread);
    if (!handle)
        return filc_ptr_forge_null();
    filc_llvm_jit_context_state* state =
        (filc_llvm_jit_context_state*)malloc(sizeof(filc_llvm_jit_context_state));
    if (!state)
        filc_safety_panic(NULL, "out of memory creating LLVM context.");
    pas_lock_construct(&state->lock);
    state->handle = (void*)handle;
    __atomic_store_n(&state->ref_count, 1u, __ATOMIC_SEQ_CST);
    filc_object* object = filc_allocate_special_with_heap(
        my_thread, sizeof(filc_llvm_jit_context_object), 16,
        FILC_SPECIAL_TYPE_LLVM_JIT, fugc_destructor_heap);
    filc_llvm_jit_context_object* context =
        (filc_llvm_jit_context_object*)filc_object_lower(object);
    context->kind = FILC_LLVM_JIT_KIND_CONTEXT;
    context->state = state;
    return filc_llvm_jit_publish(my_thread, object);
}

/* ------------------------------------------------------------------ */
/* Borrowed strings and arrays. The C API below returns pointers owned by
   the LLVM objects themselves (no Dispose needed, hence their place
   in the curated header). Wrappers copy the data into fresh Fil-C
   memory while entered; the length out-params of the C API are dropped
   because Fil-C values carry their own bounds. The two
   LLVMIntrinsicCopyOverloadedName* functions are the exception: they return
   freshly allocated strings that the full LLVM C API would release with
   LLVMDisposeMessage, which API v1 omits -- the originals leak by design
   and only the copy is returned. */

static filc_ptr filc_llvm_jit_new_sized_string(filc_thread* my_thread, const char* str,
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
        const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread); \
        filc_llvm_jit_context_object* context = NULL; \
        filc_llvm_jit_object* module = NULL; \
        filc_llvm_jit_object* parent = filc_llvm_jit_unwrap_obj( \
            my_thread, parent_ptr, kind, &context, &module); \
        const char* raw; \
        FILC_CHECK(parent, NULL, #c_name ": null parent."); \
        pas_lock_lock(&context->state->lock); \
        filc_exit(my_thread); \
        raw = api->c_name((void*)parent->handle); \
        filc_enter(my_thread); \
        filc_ptr result = filc_strdup(my_thread, raw); \
        pas_lock_unlock(&context->state->lock); \
        return result; \
    }

#define FILC_LLVM_JIT_DEFINE_BORROWED_CSTR_LEN(c_name, kind, len_type) \
    filc_ptr filc_native_##c_name(filc_thread* my_thread, filc_ptr parent_ptr) \
    { \
        const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread); \
        filc_llvm_jit_context_object* context = NULL; \
        filc_llvm_jit_object* module = NULL; \
        filc_llvm_jit_object* parent = filc_llvm_jit_unwrap_obj( \
            my_thread, parent_ptr, kind, &context, &module); \
        len_type length = 0; \
        const char* raw; \
        FILC_CHECK(parent, NULL, #c_name ": null parent."); \
        pas_lock_lock(&context->state->lock); \
        filc_exit(my_thread); \
        raw = api->c_name((void*)parent->handle, &length); \
        filc_enter(my_thread); \
        filc_ptr result = \
            filc_llvm_jit_new_sized_string(my_thread, raw, (size_t)length); \
        pas_lock_unlock(&context->state->lock); \
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
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    size_t length = 0;
    const char* raw;
    filc_exit(my_thread);
    raw = api->LLVMIntrinsicGetName(id, &length);
    filc_enter(my_thread);
    return filc_llvm_jit_new_sized_string(my_thread, raw, length);
}

filc_ptr filc_native_LLVMGetIndices(filc_thread* my_thread, filc_ptr inst_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* inst = filc_llvm_jit_unwrap_obj(
        my_thread, inst_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    unsigned count = 0;
    const unsigned* indices = NULL;
    FILC_CHECK(inst, NULL, "LLVMGetIndices: null instruction.");
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    count = api->LLVMGetNumIndices((LLVMValueRef)inst->handle);
    indices = api->LLVMGetIndices((LLVMValueRef)inst->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_ptr_create_with_object(
        my_thread, filc_allocate(my_thread, filc_mul_size(count, sizeof(unsigned))));
    if (count)
        filc_memcpy_with_exit(my_thread, filc_ptr_ptr(result), indices,
                              filc_mul_size(count, sizeof(unsigned)));
    pas_lock_unlock(&context->state->lock);
    return result;
}

filc_ptr filc_native_LLVMInstructionGetAllMetadataOtherThanDebugLoc(
    filc_thread* my_thread, filc_ptr inst_ptr, filc_ptr num_entries_ptr)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* inst = filc_llvm_jit_unwrap_obj(
        my_thread, inst_ptr, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    FILC_CHECK(inst, NULL, "LLVMInstructionGetAllMetadataOtherThanDebugLoc: null value.");
    size_t num_entries = 0;
    LLVMValueMetadataEntry* entries = NULL;
    filc_check_write(num_entries_ptr, sizeof(size_t));
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    entries = api->LLVMInstructionGetAllMetadataOtherThanDebugLoc((LLVMValueRef)inst->handle,
                                                                  &num_entries);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module,
                                         FILC_LLVM_JIT_KIND_METADATA_ENTRIES,
                                         (void*)entries);
    if (!filc_ptr_is_totally_null(result)) {
        ((filc_llvm_jit_entries_object*)filc_ptr_ptr(result))->count = num_entries;
    }
    pas_lock_unlock(&context->state->lock);
    *(size_t*)filc_ptr_ptr(num_entries_ptr) = num_entries;
    return result;
}

filc_ptr filc_native_LLVMIntrinsicCopyOverloadedName(
    filc_thread* my_thread, unsigned id, filc_ptr param_types_ptr, size_t param_count)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    LLVMTypeRef* param_types = (LLVMTypeRef*)filc_llvm_jit_decode_handle_array(
        my_thread, param_types_ptr, param_count, FILC_LLVM_JIT_KIND_TYPE,
        &context, &module);
    size_t name_length = 0;
    char* raw;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    /* Owned by LLVM (would need LLVMDisposeMessage in the full C API, which
       API v1 omits): the original leaks by design, only the copy escapes. */
    raw = api->LLVMIntrinsicCopyOverloadedName(id, param_types, param_count, &name_length);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_new_sized_string(my_thread, raw, name_length);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIntrinsicCopyOverloadedName2(
    filc_thread* my_thread, filc_ptr module_ptr, unsigned id,
    filc_ptr param_types_ptr, size_t param_count)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module_donor = NULL;
    filc_llvm_jit_object* module_obj = filc_llvm_jit_unwrap_obj(
        my_thread, module_ptr, FILC_LLVM_JIT_KIND_MODULE, &context, &module_donor);
    LLVMTypeRef* param_types = (LLVMTypeRef*)filc_llvm_jit_decode_handle_array(
        my_thread, param_types_ptr, param_count, FILC_LLVM_JIT_KIND_TYPE,
        &context, &module_donor);
    size_t name_length = 0;
    char* raw;
    FILC_CHECK(module_obj, NULL, "LLVMIntrinsicCopyOverloadedName2: null module.");
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    /* Owned by LLVM (would need LLVMDisposeMessage in the full C API, which
       API v1 omits): the original leaks by design, only the copy escapes. */
    raw = api->LLVMIntrinsicCopyOverloadedName2((LLVMModuleRef)module_obj->handle, id,
                                                param_types, param_count, &name_length);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_new_sized_string(my_thread, raw, name_length);
    pas_lock_unlock(&context->state->lock);
    return result;
}

/* ------------------------------------------------------------------ */
/* Mechanically generated thin wrappers for the curated LLVM C API.
   Thin wrappers mechanically emitted for the curated LLVM C API;
   do not edit by hand. */
filc_ptr filc_native_LLVMAddAlias2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAddAlias2((LLVMModuleRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), p2, (LLVMValueRef)filc_llvm_jit_handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMAddAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddAttributeAtIndex((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, (LLVMAttributeRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddCallSiteAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddCallSiteAttribute((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, (LLVMAttributeRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddCase(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddCase((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddClause(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddClause((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddDestination(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddDestination((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMAddFunction(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAddFunction((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1, (LLVMTypeRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMAddGlobal(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAddGlobal((LLVMModuleRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMAddGlobalIFunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMAddGlobalIFunc: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a5 = filc_llvm_jit_unwrap_obj(my_thread, p5, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAddGlobalIFunc((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1, p2, (LLVMTypeRef)filc_llvm_jit_handle_of(a3), p4, (LLVMValueRef)filc_llvm_jit_handle_of(a5));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMAddGlobalInAddressSpace(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAddGlobalInAddressSpace((LLVMModuleRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), a2, p3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMAddHandler(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddHandler((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddIncoming(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p1, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef* a2 = (LLVMBasicBlockRef*)filc_llvm_jit_decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddIncoming((LLVMValueRef)filc_llvm_jit_handle_of(a0), a1, a2, p3);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddMetadataToInst(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddMetadataToInst((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddModuleFlag(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, size_t p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a2;
    if (filc_ptr_is_totally_null(p2)) {
        FILC_CHECK(p3 == 0, NULL, "LLVMAddModuleFlag: null string with nonzero length.");
        a2 = NULL;
    } else {
        filc_check_read(p2, (size_t)p3);
        a2 = (const char*)filc_ptr_ptr(p2);
    }
    filc_llvm_jit_object* a4 = filc_llvm_jit_unwrap_obj(my_thread, p4, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddModuleFlag((LLVMModuleRef)filc_llvm_jit_handle_of(a0), p1, a2, p3, (LLVMMetadataRef)filc_llvm_jit_handle_of(a4));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddNamedMetadataOperand(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddNamedMetadataOperand((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1, (LLVMValueRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddTargetDependentFunctionAttr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddTargetDependentFunctionAttr((LLVMValueRef)filc_llvm_jit_handle_of(a0), a1, a2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMAliasGetAliasee(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAliasGetAliasee((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMAliasSetAliasee(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAliasSetAliasee((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMAlignOf(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAlignOf((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMAppendBasicBlockInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAppendBasicBlockInContext((LLVMContextRef)a0->state->handle, (LLVMValueRef)filc_llvm_jit_handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMAppendExistingBasicBlock(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAppendExistingBasicBlock((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAppendModuleInlineAsm(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMAppendModuleInlineAsm: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAppendModuleInlineAsm((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMArrayType(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMArrayType((LLVMTypeRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMArrayType2(filc_thread* my_thread, filc_ptr p0, unsigned long long p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMArrayType2((LLVMTypeRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMBFloatTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBFloatTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBasicBlockAsValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBasicBlockAsValue((LLVMBasicBlockRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBlockAddress(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBlockAddress((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAShr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAShr((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAdd((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAddrSpaceCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAddrSpaceCast((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAggregateRet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAggregateRet((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAlloca(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAlloca((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAnd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAnd((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildArrayAlloca(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildArrayAlloca((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildArrayMalloc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildArrayMalloc((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAtomicCmpXchg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, unsigned p5, bool p6)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAtomicCmpXchg((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), (LLVMValueRef)filc_llvm_jit_handle_of(a3), p4, p5, p6);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAtomicCmpXchgSyncScope(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, unsigned p5, unsigned p6)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAtomicCmpXchgSyncScope((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), (LLVMValueRef)filc_llvm_jit_handle_of(a3), p4, p5, p6);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAtomicRMW(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, unsigned p4, bool p5)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAtomicRMW((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), p1, (LLVMValueRef)filc_llvm_jit_handle_of(a2), (LLVMValueRef)filc_llvm_jit_handle_of(a3), p4, p5);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAtomicRMWSyncScope(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, unsigned p4, unsigned p5)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAtomicRMWSyncScope((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), p1, (LLVMValueRef)filc_llvm_jit_handle_of(a2), (LLVMValueRef)filc_llvm_jit_handle_of(a3), p4, p5);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildBinOp(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildBinOp((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), p1, (LLVMValueRef)filc_llvm_jit_handle_of(a2), (LLVMValueRef)filc_llvm_jit_handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildBitCast((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildBr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildBr((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCall2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a5 = filc_check_and_get_tmp_str_or_null(my_thread, p5);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCall2((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3, p4, a5);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCast(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCast((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), p1, (LLVMValueRef)filc_llvm_jit_handle_of(a2), (LLVMTypeRef)filc_llvm_jit_handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCatchPad(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCatchPad((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), a2, p3, a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCatchRet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCatchRet((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCatchSwitch(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCatchSwitch((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a2), p3, a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCleanupPad(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCleanupPad((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), a2, p3, a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCleanupRet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCleanupRet((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCondBr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCondBr((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a2), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a3));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildExactSDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildExactSDiv((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildExactUDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildExactUDiv((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildExtractElement(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildExtractElement((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildExtractValue(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildExtractValue((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), p2, a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFAdd((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFCmp(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFCmp((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), p1, (LLVMValueRef)filc_llvm_jit_handle_of(a2), (LLVMValueRef)filc_llvm_jit_handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFDiv((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFMul((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFNeg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFNeg((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFPCast((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPExt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFPExt((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPToSI(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFPToSI((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPToUI(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFPToUI((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPTrunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFPTrunc((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFRem(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFRem((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFSub((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFence(filc_thread* my_thread, filc_ptr p0, unsigned p1, bool p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFence((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), p1, p2, a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFenceSyncScope(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFenceSyncScope((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), p1, p2, a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFree(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFree((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFreeze(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFreeze((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a5 = filc_check_and_get_tmp_str_or_null(my_thread, p5);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildGEP2((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3, p4, a5);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildGEPWithNoWrapFlags(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5, unsigned p6)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a5 = filc_check_and_get_tmp_str_or_null(my_thread, p5);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildGEPWithNoWrapFlags((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3, p4, a5, p6);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildGlobalString(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildGlobalString((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), a1, a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildGlobalStringPtr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildGlobalStringPtr((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), a1, a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildICmp(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildICmp((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), p1, (LLVMValueRef)filc_llvm_jit_handle_of(a2), (LLVMValueRef)filc_llvm_jit_handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildInBoundsGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a5 = filc_check_and_get_tmp_str_or_null(my_thread, p5);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildInBoundsGEP2((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3, p4, a5);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIndirectBr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildIndirectBr((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildInsertElement(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildInsertElement((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), (LLVMValueRef)filc_llvm_jit_handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildInsertValue(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildInsertValue((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), p3, a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIntCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildIntCast((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIntCast2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, bool p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildIntCast2((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), p3, a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIntToPtr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildIntToPtr((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildInvoke2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5, filc_ptr p6, filc_ptr p7)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a5 = filc_llvm_jit_unwrap_obj(my_thread, p5, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a6 = filc_llvm_jit_unwrap_obj(my_thread, p6, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    const char* a7 = filc_check_and_get_tmp_str_or_null(my_thread, p7);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildInvoke2((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3, p4, (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a5), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a6), a7);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIsNotNull(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildIsNotNull((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIsNull(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildIsNull((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildLShr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildLShr((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildLandingPad(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildLandingPad((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), p3, a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildLoad2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildLoad2((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMalloc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildMalloc((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMemCpy(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a5 = filc_llvm_jit_unwrap_obj(my_thread, p5, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildMemCpy((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), p2, (LLVMValueRef)filc_llvm_jit_handle_of(a3), p4, (LLVMValueRef)filc_llvm_jit_handle_of(a5));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMemMove(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a5 = filc_llvm_jit_unwrap_obj(my_thread, p5, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildMemMove((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), p2, (LLVMValueRef)filc_llvm_jit_handle_of(a3), p4, (LLVMValueRef)filc_llvm_jit_handle_of(a5));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMemSet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildMemSet((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), (LLVMValueRef)filc_llvm_jit_handle_of(a3), p4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildMul((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNSWAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNSWAdd((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNSWMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNSWMul((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNSWNeg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNSWNeg((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNSWSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNSWSub((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNUWAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNUWAdd((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNUWMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNUWMul((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNUWSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNUWSub((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNeg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNeg((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNot(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNot((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildOr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildOr((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildPhi(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildPhi((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildPointerCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildPointerCast((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildPtrDiff2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildPtrDiff2((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), (LLVMValueRef)filc_llvm_jit_handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildPtrToInt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildPtrToInt((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildResume(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildResume((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildRet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildRet((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildRetVoid(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildRetVoid((LLVMBuilderRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSDiv((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSExt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSExt((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSExtOrBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSExtOrBitCast((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSIToFP(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSIToFP((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSRem(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSRem((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSelect(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSelect((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), (LLVMValueRef)filc_llvm_jit_handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildShl(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildShl((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildShuffleVector(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildShuffleVector((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), (LLVMValueRef)filc_llvm_jit_handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildStore(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildStore((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildStructGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildStructGEP2((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), p3, a4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSub((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSwitch(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSwitch((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a2), p3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildTrunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildTrunc((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildTruncOrBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildTruncOrBitCast((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildUDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildUDiv((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildUIToFP(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildUIToFP((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildURem(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildURem((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildUnreachable(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildUnreachable((LLVMBuilderRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildVAArg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildVAArg((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildXor(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildXor((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildZExt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildZExt((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildZExtOrBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildZExtOrBitCast((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMTypeRef)filc_llvm_jit_handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuilderGetDefaultFPMathTag(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMMetadataRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuilderGetDefaultFPMathTag((LLVMBuilderRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMBuilderSetDefaultFPMathTag(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMBuilderSetDefaultFPMathTag((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMMetadataRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

bool filc_native_LLVMCanValueUseFastMathFlags(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCanValueUseFastMathFlags((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMClearInsertionPosition(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMClearInsertionPosition((LLVMBuilderRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMCloneModule(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMModuleRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCloneModule((LLVMModuleRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_MODULE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstAdd((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstAddrSpaceCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstAddrSpaceCast((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstAllOnes(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstAllOnes((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstArray(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstArray((LLVMTypeRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstArray2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned long long p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstArray2((LLVMTypeRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstBitCast((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstExtractElement(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstExtractElement((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstGEP2((LLVMTypeRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), a2, p3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstGEPWithNoWrapFlags(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, unsigned p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstGEPWithNoWrapFlags((LLVMTypeRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), a2, p3, p4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstInBoundsGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstInBoundsGEP2((LLVMTypeRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), a2, p3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstInlineAsm(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, bool p3, bool p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstInlineAsm((LLVMTypeRef)filc_llvm_jit_handle_of(a0), a1, a2, p3, p4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstInsertElement(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstInsertElement((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstInt(filc_thread* my_thread, filc_ptr p0, unsigned long long p1, bool p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstInt((LLVMTypeRef)filc_llvm_jit_handle_of(a0), p1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

long long filc_native_LLVMConstIntGetSExtValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    long long result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstIntGetSExtValue((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    long long result = (long long)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned long long filc_native_LLVMConstIntGetZExtValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned long long result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstIntGetZExtValue((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned long long result = (unsigned long long)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstIntOfString(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstIntOfString((LLVMTypeRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstIntOfStringAndSize(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, unsigned p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMConstIntOfStringAndSize: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstIntOfStringAndSize((LLVMTypeRef)filc_llvm_jit_handle_of(a0), a1, p2, p3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstIntToPtr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstIntToPtr((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstMul((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNSWAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNSWAdd((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNSWMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNSWMul((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNSWNeg(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNSWNeg((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNSWSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNSWSub((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNUWAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNUWAdd((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNUWMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNUWMul((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNUWSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNUWSub((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNamedStruct(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNamedStruct((LLVMTypeRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNeg(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNeg((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNot(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNot((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNull(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNull((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstPointerCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstPointerCast((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstPointerNull(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstPointerNull((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstPtrToInt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstPtrToInt((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstReal(filc_thread* my_thread, filc_ptr p0, double p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstReal((LLVMTypeRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstRealOfString(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstRealOfString((LLVMTypeRef)filc_llvm_jit_handle_of(a0), a1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstRealOfStringAndSize(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMConstRealOfStringAndSize: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstRealOfStringAndSize((LLVMTypeRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstShuffleVector(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstShuffleVector((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMConstStringInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMConstStringInContext: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstStringInContext((LLVMContextRef)a0->state->handle, a1, p2, p3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstStringInContext2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2, bool p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMConstStringInContext2: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstStringInContext2((LLVMContextRef)a0->state->handle, a1, p2, p3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMConstStructInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    LLVMValueRef* a1 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstStructInContext((LLVMContextRef)a0->state->handle, a1, p2, p3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstSub((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstTrunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstTrunc((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstTruncOrBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstTruncOrBitCast((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMTypeRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstVector(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    LLVMValueRef* a0 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p0, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstVector(a0, p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstXor(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstXor((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstantPtrAuth(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = filc_llvm_jit_unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstantPtrAuth((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2), (LLVMValueRef)filc_llvm_jit_handle_of(a3));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMContextSetDiscardValueNames(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMContextSetDiscardValueNames((LLVMContextRef)a0->state->handle, p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

bool filc_native_LLVMContextShouldDiscardValueNames(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMContextShouldDiscardValueNames((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMCountBasicBlocks(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCountBasicBlocks((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMCountIncoming(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCountIncoming((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMCountParamTypes(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCountParamTypes((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMCountParams(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCountParams((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMCountStructElementTypes(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCountStructElementTypes((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateBasicBlockInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCreateBasicBlockInContext((LLVMContextRef)a0->state->handle, a1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMCreateBuilderInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBuilderRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCreateBuilderInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BUILDER, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateEnumAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned long long p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAttributeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCreateEnumAttribute((LLVMContextRef)a0->state->handle, p1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateStringAttribute(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3, unsigned p4)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMCreateStringAttribute: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    const char* a3;
    if (filc_ptr_is_totally_null(p3)) {
        FILC_CHECK(p4 == 0, NULL, "LLVMCreateStringAttribute: null string with nonzero length.");
        a3 = NULL;
    } else {
        filc_check_read(p3, (size_t)p4);
        a3 = (const char*)filc_ptr_ptr(p3);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAttributeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCreateStringAttribute((LLVMContextRef)a0->state->handle, a1, p2, a3, p4);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateTypeAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAttributeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCreateTypeAttribute((LLVMContextRef)a0->state->handle, p1, (LLVMTypeRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMDoubleTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMDoubleTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMDumpModule(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMDumpModule((LLVMModuleRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMDumpType(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMDumpType((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMDumpValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMDumpValue((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}


filc_ptr filc_native_LLVMFP128TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMFP128TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMFloatTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMFloatTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMFunctionType(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef* a1 = (LLVMTypeRef*)filc_llvm_jit_decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMFunctionType((LLVMTypeRef)filc_llvm_jit_handle_of(a0), a1, p2, p3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGEPGetNoWrapFlags(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMGEPNoWrapFlags result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGEPGetNoWrapFlags((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMGEPSetNoWrapFlags(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMGEPSetNoWrapFlags((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMGetAggregateElement(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetAggregateElement((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetAlignment(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetAlignment((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetAllocatedType(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetAllocatedType((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetArgOperand(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetArgOperand((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetArrayLength(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetArrayLength((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned long long filc_native_LLVMGetArrayLength2(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    uint64_t result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetArrayLength2((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned long long result = (unsigned long long)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetAtomicRMWBinOp(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAtomicRMWBinOp result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetAtomicRMWBinOp((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetAtomicSyncScopeID(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetAtomicSyncScopeID((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetAttributeCountAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetAttributeCountAtIndex((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBasicBlockParent(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetBasicBlockParent((LLVMBasicBlockRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBasicBlockTerminator(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetBasicBlockTerminator((LLVMBasicBlockRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBlockAddressBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetBlockAddressBasicBlock((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBlockAddressFunction(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetBlockAddressFunction((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBuilderContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMContextRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetBuilderContext((LLVMBuilderRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_CONTEXT, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCallBrDefaultDest(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCallBrDefaultDest((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCallBrIndirectDest(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCallBrIndirectDest((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetCallBrNumIndirectDests(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCallBrNumIndirectDests((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetCallSiteAttributeCount(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCallSiteAttributeCount((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCallSiteEnumAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAttributeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCallSiteEnumAttribute((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCallSiteStringAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, unsigned p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2;
    if (filc_ptr_is_totally_null(p2)) {
        FILC_CHECK(p3 == 0, NULL, "LLVMGetCallSiteStringAttribute: null string with nonzero length.");
        a2 = NULL;
    } else {
        filc_check_read(p2, (size_t)p3);
        a2 = (const char*)filc_ptr_ptr(p2);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAttributeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCallSiteStringAttribute((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, a2, p3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCalledFunctionType(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCalledFunctionType((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCalledValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCalledValue((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetCastOpcode(filc_thread* my_thread, filc_ptr p0, bool p1, filc_ptr p2, bool p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMOpcode result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCastOpcode((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, (LLVMTypeRef)filc_llvm_jit_handle_of(a2), p3);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetClause(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetClause((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetCmpXchgFailureOrdering(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAtomicOrdering result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCmpXchgFailureOrdering((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetCmpXchgSuccessOrdering(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAtomicOrdering result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCmpXchgSuccessOrdering((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCondition(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCondition((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetConstOpcode(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMOpcode result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetConstOpcode((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetConstantPtrAuthAddrDiscriminator(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetConstantPtrAuthAddrDiscriminator((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetConstantPtrAuthDiscriminator(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetConstantPtrAuthDiscriminator((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetConstantPtrAuthKey(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetConstantPtrAuthKey((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetConstantPtrAuthPointer(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetConstantPtrAuthPointer((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCurrentDebugLocation(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCurrentDebugLocation((LLVMBuilderRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCurrentDebugLocation2(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMMetadataRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCurrentDebugLocation2((LLVMBuilderRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetDLLStorageClass(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMDLLStorageClass result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetDLLStorageClass((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetDebugLocColumn(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetDebugLocColumn((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetDebugLocLine(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetDebugLocLine((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetElementType(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetElementType((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetEntryBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetEntryBasicBlock((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetEnumAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAttributeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetEnumAttributeAtIndex((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetEnumAttributeKind(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetEnumAttributeKind((LLVMAttributeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetEnumAttributeKindForName(filc_thread* my_thread, filc_ptr p0, size_t p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    const char* a0;
    if (filc_ptr_is_totally_null(p0)) {
        FILC_CHECK(p1 == 0, NULL, "LLVMGetEnumAttributeKindForName: null string with nonzero length.");
        a0 = NULL;
    } else {
        filc_check_read(p0, (size_t)p1);
        a0 = (const char*)filc_ptr_ptr(p0);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetEnumAttributeKindForName(a0, p1);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned long long filc_native_LLVMGetEnumAttributeValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    uint64_t result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetEnumAttributeValue((LLVMAttributeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned long long result = (unsigned long long)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetExact(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetExact((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetFCmpPredicate(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMRealPredicate result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFCmpPredicate((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetFastMathFlags(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMFastMathFlags result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFastMathFlags((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstBasicBlock((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstDbgRecord(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMDbgRecordRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstDbgRecord((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_DBG_RECORD, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstFunction(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstFunction((LLVMModuleRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstGlobal(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstGlobal((LLVMModuleRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstGlobalAlias((LLVMModuleRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstGlobalIFunc((LLVMModuleRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstInstruction(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstInstruction((LLVMBasicBlockRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstNamedMetadata(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMNamedMDNodeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstNamedMetadata((LLVMModuleRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstParam(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstParam((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstUse(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMUseRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstUse((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_USE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetFunctionCallConv(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFunctionCallConv((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetGEPSourceElementType(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetGEPSourceElementType((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetGlobalIFuncResolver(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetGlobalIFuncResolver((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetGlobalParent(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMModuleRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetGlobalParent((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_MODULE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetICmpPredicate(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMIntPredicate result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetICmpPredicate((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetIncomingBlock(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetIncomingBlock((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetIncomingValue(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetIncomingValue((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInitializer(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInitializer((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInlineAsm(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2, filc_ptr p3, size_t p4, bool p5, bool p6, unsigned p7, bool p8)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetInlineAsm: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    const char* a3;
    if (filc_ptr_is_totally_null(p3)) {
        FILC_CHECK(p4 == 0, NULL, "LLVMGetInlineAsm: null string with nonzero length.");
        a3 = NULL;
    } else {
        filc_check_read(p3, (size_t)p4);
        a3 = (const char*)filc_ptr_ptr(p3);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInlineAsm((LLVMTypeRef)filc_llvm_jit_handle_of(a0), a1, p2, a3, p4, p5, p6, p7, p8);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetInlineAsmCanUnwind(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInlineAsmCanUnwind((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetInlineAsmDialect(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMInlineAsmDialect result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInlineAsmDialect((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInlineAsmFunctionType(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInlineAsmFunctionType((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetInlineAsmHasSideEffects(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInlineAsmHasSideEffects((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetInlineAsmNeedsAlignedStack(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInlineAsmNeedsAlignedStack((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInsertBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInsertBlock((LLVMBuilderRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetInstructionCallConv(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInstructionCallConv((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetInstructionOpcode(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMOpcode result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInstructionOpcode((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInstructionParent(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInstructionParent((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetIntTypeWidth(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetIntTypeWidth((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetIntrinsicDeclaration(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, size_t p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMTypeRef* a2 = (LLVMTypeRef*)filc_llvm_jit_decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetIntrinsicDeclaration((LLVMModuleRef)filc_llvm_jit_handle_of(a0), p1, a2, p3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetIntrinsicID(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetIntrinsicID((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetIsDisjoint(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetIsDisjoint((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastBasicBlock((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastDbgRecord(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMDbgRecordRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastDbgRecord((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_DBG_RECORD, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetLastEnumAttributeKind(filc_thread* my_thread)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastEnumAttributeKind();
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastFunction(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastFunction((LLVMModuleRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastGlobal(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastGlobal((LLVMModuleRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastGlobalAlias((LLVMModuleRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastGlobalIFunc((LLVMModuleRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastInstruction(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastInstruction((LLVMBasicBlockRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastNamedMetadata(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMNamedMDNodeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastNamedMetadata((LLVMModuleRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastParam(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastParam((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetLinkage(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMLinkage result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLinkage((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


unsigned filc_native_LLVMGetMDKindIDInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetMDKindIDInContext: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetMDKindIDInContext((LLVMContextRef)a0->state->handle, a1, p2);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetMDNodeNumOperands(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetMDNodeNumOperands((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

int filc_native_LLVMGetMaskValue(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    int result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetMaskValue((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    int result = (int)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetMetadata((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetModuleContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMContextRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetModuleContext((LLVMModuleRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_CONTEXT, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetModuleFlag(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetModuleFlag: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMMetadataRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetModuleFlag((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetNNeg(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNNeg((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetNSW(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNSW((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetNUW(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNUW((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedFunction(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNamedFunction((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedFunctionWithLength(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetNamedFunctionWithLength: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNamedFunctionWithLength((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedGlobal(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNamedGlobal((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedGlobalAlias(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetNamedGlobalAlias: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNamedGlobalAlias((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedGlobalIFunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetNamedGlobalIFunc: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNamedGlobalIFunc((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedGlobalWithLength(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetNamedGlobalWithLength: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNamedGlobalWithLength((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedMetadata(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetNamedMetadata: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMNamedMDNodeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNamedMetadata((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNamedMetadataNumOperands(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNamedMetadataNumOperands((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextBasicBlock((LLVMBasicBlockRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextDbgRecord(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_DBG_RECORD, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMDbgRecordRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextDbgRecord((LLVMDbgRecordRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_DBG_RECORD, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextFunction(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextFunction((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextGlobal(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextGlobal((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextGlobalAlias((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextGlobalIFunc((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextInstruction(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextInstruction((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextNamedMetadata(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_NAMED_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMNamedMDNodeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextNamedMetadata((LLVMNamedMDNodeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextParam(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextParam((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextUse(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_USE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMUseRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextUse((LLVMUseRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_USE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNormalDest(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNormalDest((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNumArgOperands(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumArgOperands((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNumClauses(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumClauses((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNumContainedTypes(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumContainedTypes((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNumHandlers(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumHandlers((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNumIndices(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumIndices((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNumMaskElements(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumMaskElements((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

int filc_native_LLVMGetNumOperands(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    int result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumOperands((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    int result = (int)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNumSuccessors(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumSuccessors((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetOperand(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetOperand((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetOperandUse(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMUseRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetOperandUse((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_USE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetOrInsertNamedMetadata(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetOrInsertNamedMetadata: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMNamedMDNodeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetOrInsertNamedMetadata((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetOrdering(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAtomicOrdering result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetOrdering((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetParam(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetParam((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetParamParent(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetParamParent((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetParentCatchSwitch(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetParentCatchSwitch((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPersonalityFn(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPersonalityFn((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetPointerAddressSpace(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPointerAddressSpace((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPoison(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPoison((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPrefixData(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPrefixData((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousBasicBlock((LLVMBasicBlockRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousDbgRecord(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_DBG_RECORD, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMDbgRecordRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousDbgRecord((LLVMDbgRecordRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_DBG_RECORD, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousFunction(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousFunction((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousGlobal(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousGlobal((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousGlobalAlias((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousGlobalIFunc((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousInstruction(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousInstruction((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousNamedMetadata(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_NAMED_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMNamedMDNodeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousNamedMetadata((LLVMNamedMDNodeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousParam(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousParam((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPrologueData(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPrologueData((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetReturnType(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetReturnType((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetStringAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, unsigned p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2;
    if (filc_ptr_is_totally_null(p2)) {
        FILC_CHECK(p3 == 0, NULL, "LLVMGetStringAttributeAtIndex: null string with nonzero length.");
        a2 = NULL;
    } else {
        filc_check_read(p2, (size_t)p3);
        a2 = (const char*)filc_ptr_ptr(p2);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAttributeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetStringAttributeAtIndex((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, a2, p3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetSuccessor(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetSuccessor((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetSwitchDefaultDest(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetSwitchDefaultDest((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetSyncScopeID(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMGetSyncScopeID: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetSyncScopeID((LLVMContextRef)a0->state->handle, a1, p2);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetTailCallKind(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTailCallKind result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTailCallKind((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetTargetExtTypeIntParam(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTargetExtTypeIntParam((LLVMTypeRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetTargetExtTypeNumIntParams(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTargetExtTypeNumIntParams((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetTargetExtTypeNumTypeParams(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTargetExtTypeNumTypeParams((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTargetExtTypeTypeParam(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTargetExtTypeTypeParam((LLVMTypeRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetThreadLocalMode(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMThreadLocalMode result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetThreadLocalMode((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTypeAttributeValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTypeAttributeValue((LLVMAttributeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTypeByName(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTypeByName((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTypeByName2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTypeByName2((LLVMContextRef)a0->state->handle, a1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTypeContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMContextRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTypeContext((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_CONTEXT, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetTypeKind(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeKind result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTypeKind((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetUndef(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetUndef((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

int filc_native_LLVMGetUndefMaskElem(filc_thread* my_thread)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    int result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetUndefMaskElem();
    filc_enter(my_thread);
    int result = (int)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetUnnamedAddress(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMUnnamedAddr result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetUnnamedAddress((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetUnwindDest(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetUnwindDest((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetUsedValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_USE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetUsedValue((LLVMUseRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetUser(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_USE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetUser((LLVMUseRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetValueContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMContextRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetValueContext((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_CONTEXT, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetValueKind(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueKind result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetValueKind((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetVectorSize(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetVectorSize((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetVisibility(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMVisibility result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetVisibility((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetVolatile(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetVolatile((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetWeak(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetWeak((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGlobalGetValueType(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGlobalGetValueType((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMGlobalSetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMGlobalSetMetadata((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, (LLVMMetadataRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}


filc_ptr filc_native_LLVMHalfTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMHalfTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

int filc_native_LLVMHasMetadata(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    int result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMHasMetadata((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    int result = (int)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMHasPersonalityFn(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMHasPersonalityFn((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMHasPrefixData(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMHasPrefixData((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMHasPrologueData(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMHasPrologueData((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMHasUnnamedAddr(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMHasUnnamedAddr((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMInsertBasicBlockInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInsertBasicBlockInContext((LLVMContextRef)a0->state->handle, (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMInsertExistingBasicBlockAfterInsertBlock(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMInsertExistingBasicBlockAfterInsertBlock((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMInsertIntoBuilder(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMInsertIntoBuilder((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (module)
        filc_llvm_jit_builder_adopt_module(my_thread, a0, module);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMInsertIntoBuilderWithName(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMInsertIntoBuilderWithName((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1), a2);
    filc_enter(my_thread);
    if (module)
        filc_llvm_jit_builder_adopt_module(my_thread, a0, module);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMInstructionClone(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInstructionClone((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMInt128TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInt128TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMInt16TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInt16TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMInt1TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInt1TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMInt32TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInt32TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMInt64TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInt64TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMInt8TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInt8TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMIntTypeInContext(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIntTypeInContext((LLVMContextRef)a0->state->handle, p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIntrinsicGetType(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, size_t p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef* a2 = (LLVMTypeRef*)filc_llvm_jit_decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIntrinsicGetType((LLVMContextRef)a0->state->handle, p1, a2, p3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIntrinsicIsOverloaded(filc_thread* my_thread, unsigned p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIntrinsicIsOverloaded(p0);
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAAddrSpaceCastInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAAddrSpaceCastInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAAllocaInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAAllocaInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAArgument(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAArgument((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAAtomicCmpXchgInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAAtomicCmpXchgInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAAtomicRMWInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAAtomicRMWInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsABasicBlock((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABinaryOperator(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsABinaryOperator((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABitCastInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsABitCastInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABlockAddress(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsABlockAddress((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABranchInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsABranchInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACallBrInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACallBrInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACallInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACallInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACastInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACastInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACatchPadInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACatchPadInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACatchReturnInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACatchReturnInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACatchSwitchInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACatchSwitchInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACleanupPadInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACleanupPadInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACleanupReturnInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACleanupReturnInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACmpInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACmpInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstant(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstant((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantAggregateZero(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantAggregateZero((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantArray(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantArray((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantDataArray(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantDataArray((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantDataSequential(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantDataSequential((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantDataVector(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantDataVector((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantExpr(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantExpr((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantFP(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantFP((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantInt(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantInt((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantPointerNull(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantPointerNull((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantPtrAuth(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantPtrAuth((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantStruct(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantStruct((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantTokenNone(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantTokenNone((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantVector(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantVector((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsADbgDeclareInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsADbgDeclareInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsADbgInfoIntrinsic(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsADbgInfoIntrinsic((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsADbgLabelInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsADbgLabelInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsADbgVariableIntrinsic(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsADbgVariableIntrinsic((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAExtractElementInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAExtractElementInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAExtractValueInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAExtractValueInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFCmpInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFCmpInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFPExtInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFPExtInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFPToSIInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFPToSIInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFPToUIInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFPToUIInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFPTruncInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFPTruncInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFenceInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFenceInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFreezeInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFreezeInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFuncletPadInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFuncletPadInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFunction(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFunction((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGetElementPtrInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAGetElementPtrInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAGlobalAlias((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAGlobalIFunc((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalObject(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAGlobalObject((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAGlobalValue((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalVariable(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAGlobalVariable((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAICmpInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAICmpInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAIndirectBrInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAIndirectBrInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInlineAsm(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAInlineAsm((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInsertElementInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAInsertElementInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInsertValueInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAInsertValueInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInstruction(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAInstruction((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAIntToPtrInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAIntToPtrInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAIntrinsicInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAIntrinsicInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInvokeInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAInvokeInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsALandingPadInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsALandingPadInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsALoadInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsALoadInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAMemCpyInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAMemCpyInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAMemIntrinsic(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAMemIntrinsic((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAMemMoveInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAMemMoveInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAMemSetInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAMemSetInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAPHINode(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAPHINode((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAPoisonValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAPoisonValue((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAPtrToIntInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAPtrToIntInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAResumeInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAResumeInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAReturnInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAReturnInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsASExtInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsASExtInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsASIToFPInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsASIToFPInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsASelectInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsASelectInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAShuffleVectorInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAShuffleVectorInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAStoreInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAStoreInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsASwitchInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsASwitchInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsATruncInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsATruncInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUIToFPInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAUIToFPInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUnaryInstruction(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAUnaryInstruction((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUnaryOperator(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAUnaryOperator((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUndefValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAUndefValue((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUnreachableInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAUnreachableInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUser(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAUser((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAVAArgInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAVAArgInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAZExtInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAZExtInst((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsCleanup(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsCleanup((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsConditional(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsConditional((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsConstant(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsConstant((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsConstantString(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsConstantString((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsDeclaration(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsDeclaration((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsEnumAttribute(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsEnumAttribute((LLVMAttributeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsExternallyInitialized(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsExternallyInitialized((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsFunctionVarArg(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsFunctionVarArg((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsGlobalConstant(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsGlobalConstant((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsInBounds(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsInBounds((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsLiteralStruct(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsLiteralStruct((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsMultithreaded(filc_thread* my_thread)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsMultithreaded();
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsNewDbgInfoFormat(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsNewDbgInfoFormat((LLVMModuleRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsNull(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsNull((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsOpaqueStruct(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsOpaqueStruct((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsPackedStruct(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsPackedStruct((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsPoison(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsPoison((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsStringAttribute(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsStringAttribute((LLVMAttributeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsTailCall(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsTailCall((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsThreadLocal(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsThreadLocal((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsTypeAttribute(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsTypeAttribute((LLVMAttributeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsUndef(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsUndef((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMLabelTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMLabelTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMLookupIntrinsicID(filc_thread* my_thread, filc_ptr p0, size_t p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    const char* a0;
    if (filc_ptr_is_totally_null(p0)) {
        FILC_CHECK(p1 == 0, NULL, "LLVMLookupIntrinsicID: null string with nonzero length.");
        a0 = NULL;
    } else {
        filc_check_read(p0, (size_t)p1);
        a0 = (const char*)filc_ptr_ptr(p0);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMLookupIntrinsicID(a0, p1);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMMDNodeInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    LLVMValueRef* a1 = (LLVMValueRef*)filc_llvm_jit_decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMMDNodeInContext((LLVMContextRef)a0->state->handle, a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMMDNodeInContext2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    LLVMMetadataRef* a1 = (LLVMMetadataRef*)filc_llvm_jit_decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMMetadataRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMMDNodeInContext2((LLVMContextRef)a0->state->handle, a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMMDStringInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMMDStringInContext: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMMDStringInContext((LLVMContextRef)a0->state->handle, a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMMDStringInContext2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMMDStringInContext2: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMMetadataRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMMDStringInContext2((LLVMContextRef)a0->state->handle, a1, p2);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMMetadataAsValue(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMMetadataAsValue((LLVMContextRef)a0->state->handle, (LLVMMetadataRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMMetadataTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMMetadataTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMModuleCreateWithNameInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    const char* a0 = filc_check_and_get_tmp_str_or_null(my_thread, p0);
    filc_llvm_jit_context_object* a1 = filc_llvm_jit_unwrap_context(my_thread, p1);
    context = a1;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMModuleRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMModuleCreateWithNameInContext(a0, (LLVMContextRef)a1->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_MODULE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMModuleFlagEntriesGetFlagBehavior(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMModuleFlagBehavior result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMModuleFlagEntriesGetFlagBehavior((LLVMModuleFlagEntry*)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMModuleFlagEntriesGetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMMetadataRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMModuleFlagEntriesGetMetadata((LLVMModuleFlagEntry*)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMMoveBasicBlockAfter(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMMoveBasicBlockAfter((LLVMBasicBlockRef)filc_llvm_jit_handle_of(a0), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMMoveBasicBlockBefore(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMMoveBasicBlockBefore((LLVMBasicBlockRef)filc_llvm_jit_handle_of(a0), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}


filc_ptr filc_native_LLVMPPCFP128TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMPPCFP128TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMPointerType(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMPointerType((LLVMTypeRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMPointerTypeInContext(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMPointerTypeInContext((LLVMContextRef)a0->state->handle, p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMPointerTypeIsOpaque(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMPointerTypeIsOpaque((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMPositionBuilder(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMPositionBuilder((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    if (module)
        filc_llvm_jit_builder_adopt_module(my_thread, a0, module);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMPositionBuilderAtEnd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMPositionBuilderAtEnd((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (module)
        filc_llvm_jit_builder_adopt_module(my_thread, a0, module);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMPositionBuilderBefore(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMPositionBuilderBefore((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (module)
        filc_llvm_jit_builder_adopt_module(my_thread, a0, module);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMPositionBuilderBeforeDbgRecords(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMPositionBuilderBeforeDbgRecords((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a1), (LLVMValueRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    if (module)
        filc_llvm_jit_builder_adopt_module(my_thread, a0, module);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMPositionBuilderBeforeInstrAndDbgRecords(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMPositionBuilderBeforeInstrAndDbgRecords((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (module)
        filc_llvm_jit_builder_adopt_module(my_thread, a0, module);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMRemoveCallSiteEnumAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMRemoveCallSiteEnumAttribute((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMRemoveCallSiteStringAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, unsigned p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2;
    if (filc_ptr_is_totally_null(p2)) {
        FILC_CHECK(p3 == 0, NULL, "LLVMRemoveCallSiteStringAttribute: null string with nonzero length.");
        a2 = NULL;
    } else {
        filc_check_read(p2, (size_t)p3);
        a2 = (const char*)filc_ptr_ptr(p2);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMRemoveCallSiteStringAttribute((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, a2, p3);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMRemoveEnumAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMRemoveEnumAttributeAtIndex((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMRemoveStringAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, unsigned p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2;
    if (filc_ptr_is_totally_null(p2)) {
        FILC_CHECK(p3 == 0, NULL, "LLVMRemoveStringAttributeAtIndex: null string with nonzero length.");
        a2 = NULL;
    } else {
        filc_check_read(p2, (size_t)p3);
        a2 = (const char*)filc_ptr_ptr(p2);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMRemoveStringAttributeAtIndex((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, a2, p3);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMReplaceAllUsesWith(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMReplaceAllUsesWith((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMReplaceMDNodeOperandWith(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMReplaceMDNodeOperandWith((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, (LLVMMetadataRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMScalableVectorType(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMScalableVectorType((LLVMTypeRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMSetAlignment(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetAlignment((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetArgOperand(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetArgOperand((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, (LLVMValueRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetAtomicRMWBinOp(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetAtomicRMWBinOp((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetAtomicSingleThread(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetAtomicSingleThread((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetAtomicSyncScopeID(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetAtomicSyncScopeID((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetCleanup(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetCleanup((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetCmpXchgFailureOrdering(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetCmpXchgFailureOrdering((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetCmpXchgSuccessOrdering(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetCmpXchgSuccessOrdering((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetCondition(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetCondition((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetCurrentDebugLocation(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetCurrentDebugLocation((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetCurrentDebugLocation2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetCurrentDebugLocation2((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMMetadataRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetDLLStorageClass(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetDLLStorageClass((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetDataLayout(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetDataLayout((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetExact(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetExact((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetExternallyInitialized(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetExternallyInitialized((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetFastMathFlags(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetFastMathFlags((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetFunctionCallConv(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetFunctionCallConv((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetGC(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetGC((LLVMValueRef)filc_llvm_jit_handle_of(a0), a1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetGlobalConstant(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetGlobalConstant((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetGlobalIFuncResolver(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetGlobalIFuncResolver((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetInitializer(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetInitializer((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetInstDebugLocation(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetInstDebugLocation((LLVMBuilderRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetInstrParamAlignment(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetInstrParamAlignment((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetInstructionCallConv(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetInstructionCallConv((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetIsDisjoint(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetIsDisjoint((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetIsInBounds(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetIsInBounds((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetIsNewDbgInfoFormat(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetIsNewDbgInfoFormat((LLVMModuleRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetLinkage(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetLinkage((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetMetadata((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, (LLVMValueRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetModuleIdentifier(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMSetModuleIdentifier: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetModuleIdentifier((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetModuleInlineAsm(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetModuleInlineAsm((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetModuleInlineAsm2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMSetModuleInlineAsm2: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetModuleInlineAsm2((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetNNeg(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetNNeg((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetNSW(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetNSW((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetNUW(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetNUW((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetNormalDest(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetNormalDest((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetOperand(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetOperand((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, (LLVMValueRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetOrdering(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetOrdering((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetParamAlignment(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetParamAlignment((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetParentCatchSwitch(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetParentCatchSwitch((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetPersonalityFn(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetPersonalityFn((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetPrefixData(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetPrefixData((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetPrologueData(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetPrologueData((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMValueRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetSection(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetSection((LLVMValueRef)filc_llvm_jit_handle_of(a0), a1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetSourceFileName(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMSetSourceFileName: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetSourceFileName((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetSuccessor(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = filc_llvm_jit_unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetSuccessor((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1, (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetTailCall(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetTailCall((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetTailCallKind(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetTailCallKind((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetTarget(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetTarget((LLVMModuleRef)filc_llvm_jit_handle_of(a0), a1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetThreadLocal(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetThreadLocal((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetThreadLocalMode(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetThreadLocalMode((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetUnnamedAddr(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetUnnamedAddr((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetUnnamedAddress(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetUnnamedAddress((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetUnwindDest(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = filc_llvm_jit_unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetUnwindDest((LLVMValueRef)filc_llvm_jit_handle_of(a0), (LLVMBasicBlockRef)filc_llvm_jit_handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetValueName(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetValueName((LLVMValueRef)filc_llvm_jit_handle_of(a0), a1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetValueName2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMSetValueName2: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetValueName2((LLVMValueRef)filc_llvm_jit_handle_of(a0), a1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetVisibility(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetVisibility((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetVolatile(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetVolatile((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetWeak(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetWeak((LLVMValueRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMSizeOf(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMSizeOf((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMStructCreateNamed(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMStructCreateNamed((LLVMContextRef)a0->state->handle, a1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMStructGetTypeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMStructGetTypeAtIndex((LLVMTypeRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMStructSetBody(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef* a1 = (LLVMTypeRef*)filc_llvm_jit_decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMStructSetBody((LLVMTypeRef)filc_llvm_jit_handle_of(a0), a1, p2, p3);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}


filc_ptr filc_native_LLVMStructTypeInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef* a1 = (LLVMTypeRef*)filc_llvm_jit_decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMStructTypeInContext((LLVMContextRef)a0->state->handle, a1, p2, p3);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMTokenTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMTokenTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMTypeIsSized(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMTypeIsSized((LLVMTypeRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMTypeOf(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMTypeOf((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMValueAsBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMValueAsBasicBlock((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMValueAsMetadata(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMMetadataRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMValueAsMetadata((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMValueIsBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMValueIsBasicBlock((LLVMValueRef)filc_llvm_jit_handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMValueMetadataEntriesGetKind(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_METADATA_ENTRIES, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMValueMetadataEntriesGetKind((LLVMValueMetadataEntry*)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMValueMetadataEntriesGetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_METADATA_ENTRIES, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMMetadataRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMValueMetadataEntriesGetMetadata((LLVMValueMetadataEntry*)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMVectorType(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = filc_llvm_jit_unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMVectorType((LLVMTypeRef)filc_llvm_jit_handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMVoidTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMVoidTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMX86AMXTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMX86AMXTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMX86FP80TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct FilLLVMJITApi* api = filc_llvm_jit_ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = filc_llvm_jit_unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMX86FP80TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = filc_llvm_jit_wrap(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

#endif /* PAS_ENABLE_FILC */

#endif /* LIBPAS_ENABLED */
