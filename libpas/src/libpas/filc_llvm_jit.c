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

/* Runtime wrappers for the LLVM JIT API.

   Every LLVM handle crossing into Fil-C code is wrapped in a
   FILC_SPECIAL_TYPE_LLVM_JIT special object. This file implements lazy
   dlopen of libfilcllvmjit.so, the object model, the memory callbacks the
   shim routes LinkGraph sections through, and the hand written natives for
   the session API and resolution. Thin per function wrappers for the
   curated LLVM C API are mechanically emitted further below.

   The JIT memory model owns everything through the jit module object. The
   shim never allocates executable memory on its own and never calls libpas
   directly except through the callbacks installed with SetMemoryCallbacks.
   GC sections go through allocate_object with an alignment header whose
   extra pointer names the jit module. Aux sections land in the combined
   aux part of the module tail. Executable sections go to the jit heap and
   are appended to the module array. Everything else lands in the other
   part of the tail. Finalize copies linked bytes into their targets and
   hands GC payloads to finalize_object.

   CompileModule consumes both the session and the module and returns the
   compiled module, or NULL on failure. No bitcode copy is made. The triple
   always describes the running process. The session can be forgotten once
   compilation finishes. Linking is eager: before returning, CompileModule
   collects every externally visible definition in the module (functions,
   globals, aliases, ifuncs; declarations, module-local, and
   available-externally symbols are skipped, weak/linkonce linkages fail
   the compile with a clean error) and resolves each one at the LLJIT
   level (without calling any getter), so the single LinkGraph allocate
   for the module happens synchronously inside CompileModule and nothing
   is left to trigger a lazy allocate later. Callbacks verify they run on
   the compiling entered Fil-C thread (recorded per token at CompileModule
   entry; resolve callbacks, which JITed code may invoke later on any
   thread, require only an entered Fil-C thread) and fail the link cleanly
   otherwise without touching GC state; a callback that fails on another
   thread hands its error to the CompileModule thread through a locked
   per-token slot. Any second allocate for the
   same module fails the link cleanly with a sticky error, since the
   architecture relies on a single allocate reaching the
   JITLinkMemoryManager. Every eager
   result is checked: a failure destroys the half-linked module and fails
   the compile with a sticky error instead of limping on toward a later
   abort; a module with no linkable definitions fails the same way rather
   than returning permanently unlinked. The context lock covers only the
   scan, which copies every link decision (names plus function/global
   flag) into a malloc'd list; linking itself (CompileModule, the eager
   resolves, the handshake) runs unlocked and the lock is re-acquired
   only to publish the handle, so the allocate callbacks — which run
   synchronously inside CompileModule and call filc_allocate — never run
   while the lock is held. Link-phase shim calls (SetMemoryCallbacks,
   CompileModule, eager resolves) run through the teardown gate, so a
   compile racing process exit fails cleanly instead of entering the shim
   during static destruction; on failure the half-linked shim module is
   destroyed while the unpublished module object keeps owning the link
   payload (jit heap bases, GC object refs, roots, tail) for its
   destructor to reclaim. Before return the thread exits and
   runs a soft handshake so prior code writes are visible to every
   processor before any JITed address is called. On x86 the handshake is
   joined by a serializing cpuid, on arm64 by an isb; this is the cross
   modifying code fence that makes freshly linked code safely callable
   from any thread. Address lookups never hold the module lock across the
   shim call, so eager linking cannot deadlock against the allocate
   callbacks; all three lookup entry points invoke the pizlonated getter
   with the caller origin to materialize the Fil-C capability, so data
   globals keep their token. A missing caller origin fails the lookup
   cleanly (NULL plus a recorded error for GetLastError) instead of
   reaching the getter with NULL, and pollchecks bracket the getter call
   unconditionally. Lookup failures record their cause, so callers can
   distinguish an unknown symbol from a link failure.

   The shim never sees a raw GC pointer. Both the callback context and the
   jitModule token baked into compiled code are small integer tokens naming
   a malloc'd anchor that holds the owning module object plus a liveness
   flag. Decoding validates the token; a dead or unknown token fails the
   callback cleanly instead of touching freed memory. A decoded callback
   roots the module for the duration of the call (allocation root plus
   barrier, rechecked against the flag), and the destructor invalidates
   the token lock-free before tearing anything down, deferring the
   malloc'd arrays while callbacks are in flight. DestroyModule therefore
   cannot dangle against in-flight callbacks.
 */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "filc_runtime.h"

#if PAS_ENABLE_FILC

#include "filc_native.h"
#include "filc_runtime_inlines.h"
#include "fugc.h"
#include "../../../filc/llvmjit/filc_llvm_jit_api.h"
#include "jit_heap.h"
#include <dlfcn.h>
#include <sched.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

bool filc_allow_jit = false;

static const struct filc_llvm_jit_api* cached_api = NULL;
static pas_lock api_mutex = PAS_LOCK_INITIALIZER;
static bool did_init = false;
static void* so_handle = NULL;

// Teardown coordination with process exit, without ever holding a lock
// across an LLVM call.
//
// GC destructor workers call back into libfilcllvmjit.so (DestroySession,
// DestroyModule, DisposeBuilder, DisposeModule, ContextDispose). Those calls
// touch function-local static registries that the C++ runtime destroys while
// exit() runs. A worker inside such a call, or making one, while exit() runs
// those static destructors uses the registries during destruction and the
// heap corrupts. The flag plus in-flight counter close that race from the
// runtime side: every link-phase shim call made while compiling
// (SetMemoryCallbacks, CompileModule, the eager address lookups) and every
// shim call made from teardown (DestroySession, DestroyModule,
// DisposeBuilder, DisposeModule, ContextDispose) bails out once exit has
// begun, and the exit hook flips the flag and then waits (bounded; it
// never hangs exit) until no in-flight call is inside the shim anymore.
// Skipping the calls at exit only leaks; the OS
// reclaims everything. Plain C statics are used on purpose: they need no
// destruction themselves, so they stay usable until the process is gone.
//
// The check deliberately holds no lock across the LLVM call. An earlier
// revision held a global mutex from the liveness check until after the shim
// returned; but shim teardown calls can re-enter Fil-C through the
// allocate/resolve callbacks (which allocate, take other locks, and may
// trigger a collection whose destructors re-enter this very path), so one
// global mutex held across the call deadlocks, including same-thread
// self-deadlock on this non-recursive lock. Instead entry only bumps an
// atomic in-flight counter after verifying (twice, to close the race with
// exit) that exit has not begun, the LLVM call itself runs unlocked, and
// leave just drops the counter. Entry is therefore lock-free and never
// blocks, which is what destructor workers on collector threads require.
static bool jit_teardown_exiting = false;
static unsigned jit_teardown_inflight = 0;

static void jit_note_exit(void)
{
    __atomic_store_n(&jit_teardown_exiting, true, __ATOMIC_SEQ_CST);
    // Wait (yielding, without holding anything) for in-flight teardown
    // calls to leave the shim. Entry above never blocks and every
    // in-flight call is normally bounded, so this spin usually terminates
    // immediately; teardown itself never waits on the exiting thread.
    //
    // The wait is bounded: an in-flight call that blocks (for example on a
    // lock held by the exiting thread, or a long link that never yields)
    // must never hang exit. On timeout break out and leak instead; the OS
    // reclaims everything. Never spin forever here.
    for (unsigned spin = 0; spin < 2000u; ++spin) {
        if (!__atomic_load_n(&jit_teardown_inflight, __ATOMIC_SEQ_CST))
            break;
        usleep(1000);
    }
}

static void jit_register_teardown_hook(void)
{
    // Registered after every entry point that can construct one of the
    // shim's static registries, so LIFO atexit order runs this hook before
    // any of those static destructors. Repeated registration is harmless:
    // the hook body is idempotent.
    atexit(jit_note_exit);
}

static bool jit_teardown_try_enter(void)
{
    if (__atomic_load_n(&jit_teardown_exiting, __ATOMIC_SEQ_CST))
        return false;
    __atomic_fetch_add(&jit_teardown_inflight, 1u, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&jit_teardown_exiting, __ATOMIC_SEQ_CST)) {
        __atomic_fetch_sub(&jit_teardown_inflight, 1u, __ATOMIC_SEQ_CST);
        return false;
    }
    return true;
}

static void jit_teardown_leave(void)
{
    unsigned prev = __atomic_fetch_sub(&jit_teardown_inflight, 1u, __ATOMIC_SEQ_CST);
    PAS_ASSERT(prev > 0);
}

/* Runtime-side sticky last-error for CompileModule failures.
//
// The shim reports failures through its own thread-local GetLastError, but
// some CompileModule failures originate on the runtime side (for example a
// module with no linkable definitions, or an unsupported weak linkage), for
// which the shim sets nothing. Every CompileModule failure path below
// records a message here, and FilLLVMJIT_GetLastError prefers it over the
// shim string, so a Fil-C caller always sees a cause. The buffer is
// thread-local because JIT compiles run on arbitrary threads. It is cleared
// on entry to CompileModule and to the address lookups, and on CompileModule
// success, so stale failures never leak into later queries. */
static __thread char jit_sticky_error[1024];

static void jit_set_last_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(jit_sticky_error, sizeof(jit_sticky_error), format, args);
    va_end(args);
    jit_sticky_error[sizeof(jit_sticky_error) - 1] = '\0';
}

static void jit_clear_last_error(void)
{
    jit_sticky_error[0] = '\0';
}

static const char* jit_sticky_error_get(void)
{
    return jit_sticky_error[0] ? jit_sticky_error : NULL;
}

/* In-flight compile registry: which Fil-C thread owns each callback token,
   plus a locked per-token error slot for callback-thread failures.
//
// Bug #1 (callbacks assume a Fil-C thread): every *_cb below used to call
// filc_get_my_thread() unchecked and then allocate/root/barrier, which is
// only legal on an entered Fil-C thread. The allocate-path callbacks
// (allocate_jit_module/executable/object/module_data, finalize_object) run
// while JITLink materializes the LinkGraph, which the shim drives
// synchronously inside the CompileModule call that installed them (eager
// linking resolves every linkable symbol before CompileModule returns, so
// no lazy allocate is ever pending afterwards); they additionally mutate
// the module payload (tail, *_used counters) with no lock, which is only
// sound single-threaded on the compiling thread. The resolve callbacks
// (resolve_symbol/resolve_raw_pointer) are instead baked into JITed code
// and may fire at any later time on any thread running that code. Either
// way the callback must not touch GC state unless it runs on a valid
// entered Fil-C thread, and the allocate path additionally requires the
// compiling thread itself. CompileModule therefore registers (token ->
// thread) here on entry and unregisters on exit, and every callback gates
// on jit_callback_check_thread before doing anything else: a wrong-thread
// callback records a per-token error under the lock and fails the link
// cleanly (NULL, no allocation, no barrier) instead of corrupting the GC.
//
// Bug #2 (sticky error is __thread but callbacks may run elsewhere): a
// callback that fails on a different thread than the CompileModule caller
// cannot report through jit_sticky_error (the caller would never see it).
// Callbacks therefore report through jit_callback_error, which stores the
// message in the token's locked slot above; the CompileModule thread
// drains it into its own sticky via jit_compile_record_drain before
// returning, synchronously capturing the callback-thread error. When no
// record exists for the token (a callback firing after its compile is
// gone), the message falls back to this thread's sticky as best effort. */
struct jit_compile_record {
    void* token;
    filc_thread* thread;
    char error[1024];
    bool has_error;
    struct jit_compile_record* next;
};

static pas_lock jit_compile_records_lock = PAS_LOCK_INITIALIZER;
static struct jit_compile_record* jit_compile_records = NULL;

static void jit_compile_record_add(void* token, filc_thread* thread)
{
    struct jit_compile_record* rec =
        (struct jit_compile_record*)malloc(sizeof(struct jit_compile_record));
    if (!rec)
        return;
    rec->token = token;
    rec->thread = thread;
    rec->error[0] = '\0';
    rec->has_error = false;
    pas_lock_lock(&jit_compile_records_lock);
    rec->next = jit_compile_records;
    jit_compile_records = rec;
    pas_lock_unlock(&jit_compile_records_lock);
}

static void jit_compile_record_remove(void* token)
{
    if (!token)
        return;
    pas_lock_lock(&jit_compile_records_lock);
    struct jit_compile_record** link = &jit_compile_records;
    while (*link) {
        if ((*link)->token == token) {
            struct jit_compile_record* dead = *link;
            *link = dead->next;
            pas_lock_unlock(&jit_compile_records_lock);
            free(dead);
            return;
        }
        link = &(*link)->next;
    }
    pas_lock_unlock(&jit_compile_records_lock);
}

static filc_thread* jit_compile_record_thread(void* token)
{
    filc_thread* result = NULL;
    pas_lock_lock(&jit_compile_records_lock);
    struct jit_compile_record* rec;
    for (rec = jit_compile_records; rec; rec = rec->next) {
        if (rec->token == token) {
            result = rec->thread;
            break;
        }
    }
    pas_lock_unlock(&jit_compile_records_lock);
    return result;
}

/* Move any callback-thread error for this token into this thread's sticky.
   Called on the CompileModule thread after shim calls that may have run
   callbacks; the locked slot is cleared so a later query cannot see it
   twice. A recorded callback-side error overwrites the generic shim
   message because it names the actual cause. */
static void jit_compile_record_drain(void* token)
{
    char buf[1024];
    bool have = false;
    if (!token)
        return;
    pas_lock_lock(&jit_compile_records_lock);
    struct jit_compile_record* rec;
    for (rec = jit_compile_records; rec; rec = rec->next) {
        if (rec->token == token && rec->has_error) {
            memcpy(buf, rec->error, sizeof(buf));
            rec->has_error = false;
            rec->error[0] = '\0';
            have = true;
            break;
        }
    }
    pas_lock_unlock(&jit_compile_records_lock);
    if (have)
        jit_set_last_error("%s", buf);
}

/* Callback-side error report: store in the token's locked slot when the
   compile is still in flight so the CompileModule thread picks it up via
   the drain above; otherwise best-effort thread-local report. Never
   touches GC state, so it is safe on any thread. */
static void jit_callback_error(void* token, const char* format, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    buf[sizeof(buf) - 1] = '\0';
    if (token) {
        pas_lock_lock(&jit_compile_records_lock);
        struct jit_compile_record* rec;
        for (rec = jit_compile_records; rec; rec = rec->next) {
            if (rec->token == token) {
                memcpy(rec->error, buf, sizeof(rec->error));
                rec->has_error = true;
                break;
            }
        }
        bool stored = (rec != NULL);
        pas_lock_unlock(&jit_compile_records_lock);
        if (stored)
            return;
    }
    jit_set_last_error("%s", buf);
}

/* Gate for every memory/resolve callback. Returns the entered Fil-C thread
   to run on, or NULL when the callback must fail cleanly (no GC allocation,
   no rooting, no barrier): the invoking thread is not a Fil-C thread, is
   not entered, or — for the allocate path — is not the thread running
   CompileModule for this token. The failure is recorded via
   jit_callback_error so the CompileModule thread can surface it. Must be
   called before any anchor decode or lock: it holds no locks itself. */
static filc_thread* jit_callback_check_thread(void* token,
                                              bool require_compile_thread,
                                              const char* callback_name)
{
    filc_thread* my_thread = filc_get_my_thread();
    if (!my_thread) {
        jit_callback_error(token,
                           "%s: invoked on a non-Fil-C thread; failing the link cleanly.",
                           callback_name);
        return NULL;
    }
    if (!filc_thread_is_entered(my_thread)) {
        jit_callback_error(token,
                           "%s: invoking thread is not entered; failing the link cleanly.",
                           callback_name);
        return NULL;
    }
    if (require_compile_thread) {
        filc_thread* expected = jit_compile_record_thread(token);
        if (expected && my_thread != expected) {
            jit_callback_error(token,
                               "%s: invoked on a different thread than CompileModule; "
                               "failing the link cleanly.",
                               callback_name);
            return NULL;
        }
    }
    return my_thread;
}

/* Callback anchors: the tokens handed to the shim instead of raw pointers.
//
// The shim bakes the cbs.context pointer and the jitModule pointer into the
// compiled module (the per-dylib callback snapshot, the filc_jit_module
// absolute symbol, and the module-private resolve wrappers), and JITed code
// may invoke the resolve callbacks at any time afterwards. Passing the raw
// GC payload pointer there leaves no GC edge: once the allocation root is
// popped the collector is free to reclaim the module object, and any later
// callback (or a DestroyModule racing an in-flight callback) dereferences
// freed memory.
//
// Instead the runtime hands the shim small integer tokens. Each token names
// a malloc'd anchor holding the owning GC object plus a liveness flag.
// Decoding validates the token under a lock, and a dead or unknown token
// yields NULL instead of a dangling dereference. The table array itself is
// append-published and never freed (growth leaks the old array, which is
// tiny and rare), and anchor structs are reused in place with a generation
// counter, so the destructor's lock-free lookup can always find the anchor
// and mark it dead even when it cannot take the table lock. A decoded
// callback additionally roots the module object for the duration of the
// call (allocation root plus store barrier, rechecked against the liveness
// flag), so the payload verse stays valid while the callback runs; the
// destructor defers freeing the malloc'd arrays when callbacks are
// in flight. Tokens therefore never dangle: they either resolve to a live,
// rooted module or they fail cleanly. */
struct jit_anchor {
    filc_object* obj;
    void* shim_handle;
    unsigned long long gen;
    bool alive;
    unsigned inflight;
    bool has_stash;
    void** stash_jit_bases;
    size_t stash_num_jit_bases;
    filc_ptr* stash_object_refs;
    size_t stash_num_objects;
    filc_ptr* stash_roots;
    size_t stash_num_roots;
    char* stash_tail;
};

struct jit_anchor_table {
    size_t capacity;
    struct jit_anchor* slots[];
};

static pas_lock jit_anchor_lock = PAS_LOCK_INITIALIZER;
static struct jit_anchor_table* jit_anchor_table_published = NULL;
static unsigned long long jit_anchor_next_gen = 1;

static struct jit_anchor_table* jit_anchor_table_load(void)
{
    return __atomic_load_n(&jit_anchor_table_published, __ATOMIC_ACQUIRE);
}

static void jit_anchor_table_publish(struct jit_anchor_table* table)
{
    __atomic_store_n(&jit_anchor_table_published, table, __ATOMIC_RELEASE);
}

static void* jit_anchor_token_make(size_t index, unsigned long long gen)
{
    return (void*)(uintptr_t)(((gen & 0xffffffffull) << 32) | (unsigned long long)(index + 1));
}

static bool jit_anchor_token_split(void* token, size_t* index_out,
                                    unsigned long long* gen_out)
{
    uintptr_t raw = (uintptr_t)token;
    unsigned long long gen = ((unsigned long long)raw) >> 32;
    size_t index = (size_t)(((unsigned long long)raw) & 0xffffffffull);
    if (!index)
        return false;
    *index_out = index - 1;
    *gen_out = gen;
    return true;
}

/* Allocate an anchor for a freshly created jit module object. The caller
   must hold no locks. Returns the token to hand to the shim, or NULL when
   the table is exhausted (the caller fails the compile cleanly). */
static void* jit_anchor_alloc(filc_object* obj)
{
    pas_lock_lock(&jit_anchor_lock);
    struct jit_anchor_table* table = jit_anchor_table_published;
    size_t index = (size_t)-1;
    size_t scan;
    if (table) {
        for (scan = 0; scan < table->capacity; ++scan) {
            struct jit_anchor* candidate = table->slots[scan];
            if (!candidate)
                continue;
            if (!__atomic_load_n(&candidate->alive, __ATOMIC_ACQUIRE)
                && !__atomic_load_n(&candidate->inflight, __ATOMIC_ACQUIRE)) {
                index = scan;
                break;
            }
        }
    }
    if (index == (size_t)-1) {
        size_t old_cap = table ? table->capacity : 0;
        size_t new_cap = old_cap ? old_cap * 2 : 8;
        struct jit_anchor_table* grown = (struct jit_anchor_table*)malloc(
            sizeof(struct jit_anchor_table) + new_cap * sizeof(struct jit_anchor*));
        if (!grown) {
            pas_lock_unlock(&jit_anchor_lock);
            return NULL;
        }
        memset(grown->slots, 0, new_cap * sizeof(struct jit_anchor*));
        if (table)
            memcpy(grown->slots, table->slots, old_cap * sizeof(struct jit_anchor*));
        for (scan = old_cap; scan < new_cap; ++scan)
            grown->slots[scan] = NULL;
        grown->capacity = new_cap;
        /* Old tables are intentionally never freed: lock-free readers may
           still hold them. Growth is rare and the leak is bounded. */
        jit_anchor_table_publish(grown);
        table = grown;
        index = old_cap;
    }
    struct jit_anchor* anchor = table->slots[index];
    if (!anchor) {
        anchor = (struct jit_anchor*)calloc(1, sizeof(struct jit_anchor));
        if (!anchor) {
            pas_lock_unlock(&jit_anchor_lock);
            return NULL;
        }
        table->slots[index] = anchor;
    }
    anchor->obj = obj;
    anchor->shim_handle = NULL;
    anchor->gen = jit_anchor_next_gen++;
    if (!anchor->gen)
        anchor->gen = jit_anchor_next_gen++;
    __atomic_store_n(&anchor->inflight, 0u, __ATOMIC_RELAXED);
    anchor->has_stash = false;
    anchor->stash_jit_bases = NULL;
    anchor->stash_num_jit_bases = 0;
    anchor->stash_object_refs = NULL;
    anchor->stash_num_objects = 0;
    anchor->stash_roots = NULL;
    anchor->stash_num_roots = 0;
    anchor->stash_tail = NULL;
    __atomic_store_n(&anchor->alive, true, __ATOMIC_RELEASE);
    void* token = jit_anchor_token_make(index, anchor->gen);
    pas_lock_unlock(&jit_anchor_lock);
    return token;
}

static void jit_anchor_fill_shim(void* token, void* shim_handle)
{
    size_t index;
    unsigned long long gen;
    if (!jit_anchor_token_split(token, &index, &gen))
        return;
    pas_lock_lock(&jit_anchor_lock);
    struct jit_anchor_table* table = jit_anchor_table_published;
    if (table && index < table->capacity) {
        struct jit_anchor* anchor = table->slots[index];
        if (anchor && anchor->gen == gen)
            anchor->shim_handle = shim_handle;
    }
    pas_lock_unlock(&jit_anchor_lock);
}

/* Lock-free anchor lookup for teardown paths, which must never block. The
   returned anchor (if any) matches both the GC object and the claimed shim
   handle; the caller must still treat every field as concurrently mutable
   and use atomics. */
static struct jit_anchor* jit_anchor_find_lockfree(filc_object* obj, void* shim_handle)
{
    struct jit_anchor_table* table = jit_anchor_table_load();
    size_t index;
    if (!table)
        return NULL;
    for (index = 0; index < table->capacity; ++index) {
        struct jit_anchor* anchor =
            (struct jit_anchor*)__atomic_load_n(&table->slots[index], __ATOMIC_ACQUIRE);
        if (!anchor)
            continue;
        filc_object* anchor_obj =
            (filc_object*)__atomic_load_n(&anchor->obj, __ATOMIC_ACQUIRE);
        void* anchor_shim =
            (void*)__atomic_load_n(&anchor->shim_handle, __ATOMIC_ACQUIRE);
        if (anchor_obj == obj && anchor_shim == shim_handle)
            return anchor;
    }
    return NULL;
}

/* Decode a shim token to the live module payload, rooting the owning object
   for the duration of the callback. Returns NULL for dead or unknown
   tokens, or when the module died concurrently. The caller must invoke
   jit_anchor_release with the same token when done. */
static filc_llvm_jit_jit_module_object* jit_anchor_acquire(filc_thread* my_thread,
                                                           void* token)
{
    size_t index;
    unsigned long long gen;
    struct jit_anchor* anchor;
    filc_object* obj;
    if (!jit_anchor_token_split(token, &index, &gen))
        return NULL;
    pas_lock_lock(&jit_anchor_lock);
    struct jit_anchor_table* table = jit_anchor_table_published;
    if (!table || index >= table->capacity) {
        pas_lock_unlock(&jit_anchor_lock);
        return NULL;
    }
    anchor = table->slots[index];
    if (!anchor || anchor->gen != gen
        || !__atomic_load_n(&anchor->alive, __ATOMIC_ACQUIRE)) {
        pas_lock_unlock(&jit_anchor_lock);
        return NULL;
    }
    __atomic_fetch_add(&anchor->inflight, 1u, __ATOMIC_ACQ_REL);
    if (!__atomic_load_n(&anchor->alive, __ATOMIC_ACQUIRE)) {
        if (__atomic_sub_fetch(&anchor->inflight, 1u, __ATOMIC_ACQ_REL) == 0u) {
            /* Last one out after death: finish deferred teardown now. */
            if (anchor->has_stash) {
                size_t raw;
                for (raw = 0; raw < anchor->stash_num_jit_bases; ++raw) {
                    if (anchor->stash_jit_bases[raw])
                        jit_heap_deallocate(anchor->stash_jit_bases[raw]);
                }
                free(anchor->stash_jit_bases);
                free(anchor->stash_object_refs);
                free(anchor->stash_roots);
                free(anchor->stash_tail);
                anchor->has_stash = false;
            }
        }
        pas_lock_unlock(&jit_anchor_lock);
        return NULL;
    }
    obj = anchor->obj;
    /* Root the module for the callback: the shim holds only the token, so
       without this edge a concurrent collection could sweep the module out
       from under us mid-callback. The barrier publishes the edge to a
       concurrent marker; the liveness recheck below closes the race where
       the mark already passed. LIFO discipline holds because callbacks nest
       strictly. */
    filc_push_allocation_root(my_thread, obj);
    if (filc_object_is_markable(obj))
        filc_store_barrier(my_thread, obj);
    if (!__atomic_load_n(&anchor->alive, __ATOMIC_ACQUIRE)) {
        filc_pop_allocation_root(my_thread, obj);
        if (__atomic_sub_fetch(&anchor->inflight, 1u, __ATOMIC_ACQ_REL) == 0u) {
            if (anchor->has_stash) {
                size_t raw;
                for (raw = 0; raw < anchor->stash_num_jit_bases; ++raw) {
                    if (anchor->stash_jit_bases[raw])
                        jit_heap_deallocate(anchor->stash_jit_bases[raw]);
                }
                free(anchor->stash_jit_bases);
                free(anchor->stash_object_refs);
                free(anchor->stash_roots);
                free(anchor->stash_tail);
                anchor->has_stash = false;
            }
        }
        pas_lock_unlock(&jit_anchor_lock);
        return NULL;
    }
    pas_lock_unlock(&jit_anchor_lock);
    return (filc_llvm_jit_jit_module_object*)filc_object_lower(obj);
}

static void jit_anchor_release(filc_thread* my_thread, void* token)
{
    size_t index;
    unsigned long long gen;
    struct jit_anchor* anchor;
    if (!jit_anchor_token_split(token, &index, &gen))
        return;
    pas_lock_lock(&jit_anchor_lock);
    struct jit_anchor_table* table = jit_anchor_table_published;
    if (!table || index >= table->capacity) {
        pas_lock_unlock(&jit_anchor_lock);
        return;
    }
    anchor = table->slots[index];
    if (!anchor || anchor->gen != gen) {
        /* Slot was recycled for another module; teardown (if any) is owned
           by whoever marked it dead. */
        pas_lock_unlock(&jit_anchor_lock);
        return;
    }
    filc_object* obj = anchor->obj;
    filc_pop_allocation_root(my_thread, obj);
    unsigned remaining = __atomic_sub_fetch(&anchor->inflight, 1u, __ATOMIC_ACQ_REL);
    if (remaining == 0u && !__atomic_load_n(&anchor->alive, __ATOMIC_ACQUIRE)
        && anchor->has_stash) {
        size_t raw;
        for (raw = 0; raw < anchor->stash_num_jit_bases; ++raw) {
            if (anchor->stash_jit_bases[raw])
                jit_heap_deallocate(anchor->stash_jit_bases[raw]);
        }
        free(anchor->stash_jit_bases);
        anchor->stash_jit_bases = NULL;
        anchor->stash_num_jit_bases = 0;
        free(anchor->stash_object_refs);
        anchor->stash_object_refs = NULL;
        anchor->stash_num_objects = 0;
        free(anchor->stash_roots);
        anchor->stash_roots = NULL;
        anchor->stash_num_roots = 0;
        free(anchor->stash_tail);
        anchor->stash_tail = NULL;
        anchor->has_stash = false;
    }
    pas_lock_unlock(&jit_anchor_lock);
}

/* Abandon an anchor for a compile that never published a module (failure
   paths in CompileModule). No callback can be in flight: linking is
   synchronous and no code escaped, so the slot is simply marked dead and
   made reusable; the payload itself is left for the collector. */
static void jit_anchor_abandon(void* token)
{
    size_t index;
    unsigned long long gen;
    struct jit_anchor* anchor;
    if (!jit_anchor_token_split(token, &index, &gen))
        return;
    pas_lock_lock(&jit_anchor_lock);
    struct jit_anchor_table* table = jit_anchor_table_published;
    if (table && index < table->capacity) {
        anchor = table->slots[index];
        if (anchor && anchor->gen == gen) {
            __atomic_store_n(&anchor->alive, false, __ATOMIC_RELEASE);
            if (__atomic_load_n(&anchor->inflight, __ATOMIC_ACQUIRE) == 0u) {
                anchor->shim_handle = NULL;
                anchor->has_stash = false;
            }
        }
    }
    pas_lock_unlock(&jit_anchor_lock);
}

static const struct filc_llvm_jit_api* ensure_api_slow(filc_thread* my_thread);
static const struct filc_llvm_jit_api* ensure_api(filc_thread* my_thread)
{
    FILC_CHECK(filc_allow_jit, NULL,
               "Fil-C LLVM JIT API used but JIT support is not enabled for this process.");
    const struct filc_llvm_jit_api* api = cached_api;
    if (api)
        return api;
    return ensure_api_slow(my_thread);
}

static bool try_load_one(const char* path)
{
    if (!path || !path[0])
        return false;
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle)
        return false;
    const struct filc_llvm_jit_api* (*get_api)(void) =
        (const struct filc_llvm_jit_api* (*)(void))dlsym(handle, "FilLLVMJIT_GetApi");
    if (!get_api) {
        dlclose(handle);
        return false;
    }
    const struct filc_llvm_jit_api* api = get_api();
    if (!api || api->version != FILC_LLVM_JIT_API_VERSION) {
        dlclose(handle);
        return false;
    }
    cached_api = api;
    so_handle = handle;
    return true;
}

static const struct filc_llvm_jit_api* ensure_api_slow(filc_thread* my_thread)
{
    pas_lock_lock(&api_mutex);
    if (did_init) {
        const struct filc_llvm_jit_api* api = cached_api;
        pas_lock_unlock(&api_mutex);
        PAS_ASSERT(api);
        return api;
    }
    const char* env = secure_getenv("FILC_LLVMJIT_SO");
    if (env && env[0])
        try_load_one(env);
    if (!cached_api) {
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
    PAS_ASSERT(cached_api);
    const struct filc_llvm_jit_api* api = cached_api;
    did_init = true;
    pas_lock_unlock(&api_mutex);
    PAS_UNUSED_PARAM(my_thread);
    return api;
}

static void drop_context_state(filc_llvm_jit_context_state* state)
{
    if (!state)
        return;
    unsigned prev = __atomic_fetch_sub(&state->ref_count, 1u, __ATOMIC_SEQ_CST);
    if (prev != 1)
        return;
    /* no log */
    const struct filc_llvm_jit_api* api = cached_api;
    if (state->handle && api && api->ContextDispose) {
        if (jit_teardown_try_enter()) {
            api->ContextDispose((LLVMContextRef)state->handle);
            jit_teardown_leave();
        }
    }
    free(state);
}

static void drop_module_state(filc_llvm_jit_module_state* module_state)
{
    if (!module_state)
        return;
    unsigned prev = __atomic_fetch_sub(&module_state->ref_count, 1u, __ATOMIC_SEQ_CST);
    if (prev != 1)
        return;
    /* no log */
    const struct filc_llvm_jit_api* api = cached_api;
    if (module_state->handle && api && api->DisposeModule) {
        if (jit_teardown_try_enter()) {
            api->DisposeModule((LLVMModuleRef)module_state->handle);
            jit_teardown_leave();
        }
    }
    drop_context_state(module_state->context_state);
    free(module_state);
}

/* Payload sizes for wrap_handle below (LP64: 16-byte filc_ptr, 16-aligned
   structs). The builder once allocated only sizeof(value) (80B) and
   overflowed once the adopted-module field grew it to 96B, heap-corrupting
   under parallel GC stress (scribble+rage). These assertions pin every
   wrap_handle size so any struct growth fails the build instead of
   corrupting the heap; audit them together with the kind switch. */
_Static_assert(sizeof(struct filc_llvm_jit_object) == 80,
               "filc_llvm_jit_object payload size changed; re-audit wrap_handle");
_Static_assert(sizeof(struct filc_llvm_jit_builder_object) == 96,
               "filc_llvm_jit_builder_object payload size changed; re-audit wrap_handle");
_Static_assert(sizeof(struct filc_llvm_jit_value_object) == 80,
               "filc_llvm_jit_value_object payload size changed; re-audit wrap_handle");
_Static_assert(sizeof(struct filc_llvm_jit_entries_object) == 96,
               "filc_llvm_jit_entries_object payload size changed; re-audit wrap_handle");
_Static_assert(sizeof(struct filc_llvm_jit_module_object) == 80,
               "filc_llvm_jit_module_object payload size changed; re-audit wrap_handle");

static filc_ptr wrap_handle(filc_thread* my_thread,
                            filc_llvm_jit_context_object* context_obj,
                            filc_llvm_jit_object* module_obj,
                            filc_llvm_jit_kind kind,
                            void* handle)
{
    size_t size;
    switch (kind) {
    case FILC_LLVM_JIT_KIND_MODULE:
        size = sizeof(filc_llvm_jit_module_object);
        PAS_ASSERT(size == sizeof(struct filc_llvm_jit_object));
        break;
    case FILC_LLVM_JIT_KIND_METADATA_ENTRIES:
    case FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES:
        size = sizeof(filc_llvm_jit_entries_object);
        break;
    case FILC_LLVM_JIT_KIND_BUILDER:
        size = sizeof(filc_llvm_jit_builder_object);
        PAS_ASSERT(size == 96);
        PAS_ASSERT(size > sizeof(filc_llvm_jit_value_object));
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
        size = sizeof(filc_llvm_jit_value_object);
        PAS_ASSERT(size == sizeof(struct filc_llvm_jit_object));
        break;
    default:
        filc_safety_panic(NULL, "wrap_handle: unexpected kind %u.", (unsigned)kind);
    }
    if (!handle)
        return filc_ptr_forge_null();
    FILC_CHECK(context_obj, NULL,
               "Fil-C LLVM JIT API: null LLVM context.");
    filc_llvm_jit_context_state* context_state = context_obj->state;
    filc_ptr context_ref = filc_ptr_create_with_special_object(my_thread,
        filc_object_for_special_payload(context_obj));
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
        module_ref = filc_ptr_create_with_special_object(my_thread,
            filc_object_for_special_payload(module_obj));
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
    if (kind == FILC_LLVM_JIT_KIND_BUILDER)
        ((filc_llvm_jit_builder_object*)payload)->adopted = NULL;
    if (kind == FILC_LLVM_JIT_KIND_METADATA_ENTRIES
        || kind == FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES)
        ((filc_llvm_jit_entries_object*)payload)->count = 0;
    return filc_ptr_create_with_special_object(my_thread, object);
}

static void builder_adopt_module(filc_thread* my_thread, filc_llvm_jit_object* builder,
                                 filc_llvm_jit_object* module_obj)
{
    PAS_ASSERT(builder->kind == FILC_LLVM_JIT_KIND_BUILDER);
    filc_llvm_jit_builder_object* b = (filc_llvm_jit_builder_object*)builder;
    filc_llvm_jit_module_state* new_state =
        module_obj ? module_obj->module_state : NULL;
    if (builder->module_state == new_state) {
        filc_llvm_jit_object* want = module_obj;
        filc_llvm_jit_object* have =
            __atomic_load_n(&b->adopted, __ATOMIC_ACQUIRE);
        if (have != want) {
            __atomic_store_n(&b->adopted, want, __ATOMIC_RELEASE);
            if (want)
                filc_store_barrier(my_thread, filc_object_for_special_payload(want));
        }
        return;
    }
    if (builder->module_state)
        drop_module_state(builder->module_state);
    builder->module_state = new_state;
    if (new_state)
        __atomic_fetch_add(&new_state->ref_count, 1u, __ATOMIC_SEQ_CST);
    __atomic_store_n(&b->adopted, module_obj, __ATOMIC_RELEASE);
    if (module_obj) {
        filc_store_barrier(my_thread, filc_object_for_special_payload(module_obj));
        filc_ptr keep = filc_ptr_create_with_special_object(my_thread,
            filc_object_for_special_payload(module_obj));
        filc_flight_ptr_store(my_thread, &builder->module_ref, keep);
    } else {
        filc_flight_ptr_store(my_thread, &builder->module_ref,
                              filc_ptr_forge_null());
    }
}

static filc_llvm_jit_object* unwrap_obj(filc_thread* my_thread, filc_ptr ptr,
                                        filc_llvm_jit_kind expected,
                                        filc_llvm_jit_context_object** context_inout,
                                        filc_llvm_jit_object** module_inout)
{
    PAS_UNUSED_PARAM(my_thread);
    if (filc_ptr_is_totally_null(ptr))
        return NULL;
    filc_check_access_special(ptr, FILC_SPECIAL_TYPE_LLVM_JIT);
    filc_llvm_jit_object* payload = (filc_llvm_jit_object*)filc_ptr_ptr(ptr);
    FILC_CHECK(payload->kind == expected, NULL,
               "Fil-C LLVM JIT API: object kind mismatch.");
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

static filc_llvm_jit_context_object* unwrap_context(filc_thread* my_thread, filc_ptr ptr)
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

static filc_llvm_jit_session_object* unwrap_session(filc_thread* my_thread, filc_ptr ptr)
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

static filc_llvm_jit_jit_module_object* unwrap_jit_module(filc_thread* my_thread, filc_ptr ptr)
{
    PAS_UNUSED_PARAM(my_thread);
    if (filc_ptr_is_totally_null(ptr))
        return NULL;
    filc_check_access_special(ptr, FILC_SPECIAL_TYPE_LLVM_JIT);
    filc_llvm_jit_jit_module_object* payload =
        (filc_llvm_jit_jit_module_object*)filc_ptr_ptr(ptr);
    FILC_CHECK(payload->kind == FILC_LLVM_JIT_KIND_JIT_MODULE, NULL,
               "Fil-C LLVM JIT API: expected a JIT module object.");
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
    if (filc_ptr_is_totally_null(array_ptr)) {
        FILC_CHECK(!count, NULL,
                   "Fil-C LLVM JIT API: null array with nonzero count.");
        return NULL;
    }
    result = (void**)filc_bmalloc_allocate_tmp(
        my_thread, filc_mul_size(count, sizeof(void*)));
    for (index = 0; index < count; ++index) {
        filc_ptr element = filc_load_ptr(my_thread, array_ptr,
                                         (ptrdiff_t)filc_mul_size(index, sizeof(void*)));
        filc_llvm_jit_object* payload = unwrap_obj(
            my_thread, element, expected, context_inout, module_inout);
        result[index] = handle_of(payload);
    }
    return result;
}

void filc_llvm_jit_destruct(void* raw_payload)
{
    filc_llvm_jit_kind kind = *(filc_llvm_jit_kind*)raw_payload;
    // no log
    //
    // Every path below must tolerate running twice on the same payload.
    // The collector tears down destructor-heap objects on parallel workers
    // and teardown order is arbitrary, so a second pass over an already
    // torn down payload must be a no-op. Each owned resource is claimed
    // with an atomic exchange before it is released, so exactly one caller
    // observes the non-null value and performs the release. Lock free
    // claiming is deliberate: post-main teardown must not depend on locks
    // that may belong to dead threads.
    switch (kind) {
    case FILC_LLVM_JIT_KIND_CONTEXT: {
        filc_llvm_jit_context_object* object = (filc_llvm_jit_context_object*)raw_payload;
        filc_llvm_jit_context_state* state = __atomic_exchange_n(
            &object->state, (filc_llvm_jit_context_state*)NULL, __ATOMIC_ACQ_REL);
        if (state)
            drop_context_state(state);
        return;
    }
    case FILC_LLVM_JIT_KIND_MODULE: {
        filc_llvm_jit_module_object* object = (filc_llvm_jit_module_object*)raw_payload;
        filc_llvm_jit_module_state* module_state = __atomic_exchange_n(
            &object->base.module_state, (filc_llvm_jit_module_state*)NULL,
            __ATOMIC_ACQ_REL);
        filc_llvm_jit_context_state* context_state = __atomic_exchange_n(
            &object->base.state, (filc_llvm_jit_context_state*)NULL,
            __ATOMIC_ACQ_REL);
        if (module_state)
            drop_module_state(module_state);
        if (context_state)
            drop_context_state(context_state);
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
        const struct filc_llvm_jit_api* api = cached_api;
        filc_llvm_jit_object* object = (filc_llvm_jit_object*)raw_payload;
        void* handle = __atomic_exchange_n(
            &object->handle, (void*)NULL, __ATOMIC_ACQ_REL);
        if (kind == FILC_LLVM_JIT_KIND_BUILDER && handle && api && api->DisposeBuilder) {
            if (jit_teardown_try_enter()) {
                api->DisposeBuilder((LLVMBuilderRef)handle);
                jit_teardown_leave();
            }
        }
        if (kind == FILC_LLVM_JIT_KIND_BUILDER) {
            (void)__atomic_exchange_n(
                &((filc_llvm_jit_builder_object*)object)->adopted,
                (filc_llvm_jit_object*)NULL, __ATOMIC_ACQ_REL);
        }
        filc_llvm_jit_module_state* module_state = __atomic_exchange_n(
            &object->module_state, (filc_llvm_jit_module_state*)NULL,
            __ATOMIC_ACQ_REL);
        filc_llvm_jit_context_state* context_state = __atomic_exchange_n(
            &object->state, (filc_llvm_jit_context_state*)NULL,
            __ATOMIC_ACQ_REL);
        if (module_state)
            drop_module_state(module_state);
        if (context_state)
            drop_context_state(context_state);
        return;
    }
    case FILC_LLVM_JIT_KIND_JIT_SESSION: {
        filc_llvm_jit_session_object* object =
            (filc_llvm_jit_session_object*)raw_payload;
        const struct filc_llvm_jit_api* api = cached_api;
        void* handle = __atomic_exchange_n(
            &object->handle, (void*)NULL, __ATOMIC_ACQ_REL);
        if (handle && api && api->DestroySession) {
            if (jit_teardown_try_enter()) {
                api->DestroySession((ShimJit)handle);
                jit_teardown_leave();
            }
        }
        return;
    }
    case FILC_LLVM_JIT_KIND_JIT_MODULE: {
        filc_llvm_jit_jit_module_object* object =
            (filc_llvm_jit_jit_module_object*)raw_payload;
        const struct filc_llvm_jit_api* api = cached_api;
        void* handle = __atomic_exchange_n(
            &object->handle, (void*)NULL, __ATOMIC_ACQ_REL);
        if (handle && api && api->DestroyModule) {
            if (jit_teardown_try_enter()) {
                api->DestroyModule((ShimJitModule)handle);
                jit_teardown_leave();
            }
        }
        size_t num_jit_bases = __atomic_exchange_n(
            &object->num_jit_bases, (size_t)0, __ATOMIC_ACQ_REL);
        void** jit_bases = __atomic_exchange_n(
            &object->jit_bases, (void**)NULL, __ATOMIC_ACQ_REL);
        size_t num_objects = __atomic_exchange_n(
            &object->num_objects, (size_t)0, __ATOMIC_ACQ_REL);
        filc_ptr* object_refs = __atomic_exchange_n(
            &object->object_refs, (filc_ptr*)NULL, __ATOMIC_ACQ_REL);
        size_t num_roots = __atomic_exchange_n(
            &object->num_roots, (size_t)0, __ATOMIC_ACQ_REL);
        filc_ptr* roots = __atomic_exchange_n(
            &object->roots, (filc_ptr*)NULL, __ATOMIC_ACQ_REL);
        char* tail = __atomic_exchange_n(
            &object->tail, (char*)NULL, __ATOMIC_ACQ_REL);
        object->jit_bases_capacity = 0;
        object->objects_capacity = 0;
        object->roots_capacity = 0;
        PAS_UNUSED_PARAM(num_objects);
        PAS_UNUSED_PARAM(num_roots);
        /* Invalidate the callback token first, lock-free and
           unconditionally: after this store, decoding the baked token fails
           instead of dereferencing this payload. The store must not depend
           on any lock (teardown runs on collector workers and must not
           block on locks that may belong to dead threads). Disposition of
           the exchanged arrays still needs the table lock, so it is
           attempted with try-lock only: on failure the arrays leak (safe)
           rather than risk deadlock. When callbacks are in flight their
           allocation roots (pushed at decode time) keep this payload's
           verse alive; only the malloc'd arrays below are at risk, so an
           in-flight callback stashes them in the anchor and the last one
           out frees them. */
        filc_object* self_obj = filc_object_for_special_payload(object);
        struct jit_anchor* anchor = jit_anchor_find_lockfree(self_obj, handle);
        if (anchor)
            __atomic_store_n(&anchor->alive, false, __ATOMIC_RELEASE);
        bool disposed = false;
        bool lock_acquired = false;
        if (anchor && pas_lock_try_lock(&jit_anchor_lock)) {
            lock_acquired = true;
            struct jit_anchor* locked = NULL;
            struct jit_anchor_table* table = jit_anchor_table_published;
            size_t scan;
            for (scan = 0; table && scan < table->capacity; ++scan) {
                struct jit_anchor* candidate = table->slots[scan];
                if (candidate == anchor) {
                    locked = candidate;
                    break;
                }
            }
            if (locked && locked->obj == self_obj && locked->shim_handle == handle) {
                if (__atomic_load_n(&locked->inflight, __ATOMIC_ACQUIRE) == 0u) {
                    size_t i;
                    for (i = 0; i < num_jit_bases; ++i) {
                        if (jit_bases && jit_bases[i])
                            jit_heap_deallocate(jit_bases[i]);
                    }
                    free(jit_bases);
                    free(object_refs);
                    free(roots);
                    free(tail);
                    disposed = true;
                } else {
                    locked->has_stash = true;
                    locked->stash_jit_bases = jit_bases;
                    locked->stash_num_jit_bases = num_jit_bases;
                    locked->stash_object_refs = object_refs;
                    locked->stash_num_objects = num_objects;
                    locked->stash_roots = roots;
                    locked->stash_num_roots = num_roots;
                    locked->stash_tail = tail;
                    disposed = true;
                }
            }
            pas_lock_unlock(&jit_anchor_lock);
        }
        if (!disposed && (!anchor || lock_acquired)) {
            /* Either there is no token to worry about, or we hold the
               table lock and know no callback is in flight (it stashed
               otherwise). Only when the try-lock itself failed do we leak
               the arrays rather than risk freeing them under a live
               callback. */
            size_t i;
            for (i = 0; i < num_jit_bases; ++i) {
                if (jit_bases && jit_bases[i])
                    jit_heap_deallocate(jit_bases[i]);
            }
            free(jit_bases);
            free(object_refs);
            free(roots);
            free(tail);
        }
        return;
    }
    default:
        // Unrecognized kind: not a fully constructed wrapper (or not one at
        // all). There is nothing safe to release, so bail out quietly and
        // let sweep reclaim the verse memory.
        return;
    }
}

struct eager_symbol {
    char* name;
    bool is_function;
};

static void push_eager_symbol(struct eager_symbol** names, size_t* num, size_t* cap,
                              const char* name, bool is_function)
{
    if (!name || !name[0])
        return;
    if (*num == *cap) {
        size_t next = *cap ? *cap * 2 : 8;
        struct eager_symbol* next_names =
            (struct eager_symbol*)realloc(*names, next * sizeof(struct eager_symbol));
        if (!next_names)
            filc_safety_panic(NULL, "out of memory collecting JIT eager symbols.");
        *names = next_names;
        *cap = next;
    }
    char* copy = strdup(name);
    if (!copy)
        filc_safety_panic(NULL, "out of memory collecting JIT eager symbols.");
    (*names)[*num].name = copy;
    (*names)[*num].is_function = is_function;
    (*num)++;
}

static void free_eager_symbols(struct eager_symbol* names, size_t num)
{
    size_t index;
    for (index = 0; index < num; ++index)
        free(names[index].name);
    free(names);
}

/* Fail CompileModule cleanly: the sticky error must already be set by the
   caller. A callback that failed on another thread may additionally have
   recorded a more specific cause in the token's locked slot; drain it so
   the callback-thread error lands on this (CompileModule) thread, then
   drop the registry entry. Destroys the half-linked shim module when one
   exists (through the teardown gate, holding no lock), abandons the
   callback token (no callback can be in flight: linking is synchronous
   and no code escaped), frees the eager list, drops the allocation root,
   and returns null. The payload itself is left for the collector; its
   destructor will find the abandoned anchor and skip the shim call.
   Publish happens only after eager linking and the handshake succeed, so
   on every failure path the module handle is still NULL here: there is no
   double destroy, and the link payload appended to the module before the
   failure (jit heap bases, GC object refs, roots, tail) stays owned by
   the module object for the destructor to reclaim. No leak or orphan. */
static filc_ptr jit_compile_fail(filc_thread* my_thread, filc_object* jit_object,
                                 void* token, void* shim_module,
                                 struct eager_symbol* eager, size_t num_eager)
{
    const struct filc_llvm_jit_api* api = cached_api;
    if (shim_module && api && api->DestroyModule) {
        if (jit_teardown_try_enter()) {
            api->DestroyModule((ShimJitModule)shim_module);
            jit_teardown_leave();
        }
    }
    if (token) {
        jit_compile_record_drain(token);
        jit_compile_record_remove(token);
        jit_anchor_abandon(token);
    }
    free_eager_symbols(eager, num_eager);
    filc_pop_allocation_root(my_thread, jit_object);
    return filc_ptr_forge_null();
}

static void push_jit_base(filc_llvm_jit_jit_module_object* mod, void* base)
{
    if (mod->num_jit_bases == mod->jit_bases_capacity) {
        size_t next = mod->jit_bases_capacity ? mod->jit_bases_capacity * 2 : 8;
        void** next_bases = (void**)realloc(mod->jit_bases, next * sizeof(void*));
        if (!next_bases)
            filc_safety_panic(NULL, "out of memory tracking JIT executable.");
        mod->jit_bases = next_bases;
        mod->jit_bases_capacity = next;
    }
    mod->jit_bases[mod->num_jit_bases++] = base;
}

static void push_object_ref(filc_thread* my_thread,
                             filc_llvm_jit_jit_module_object* mod, filc_ptr ref)
{
    if (mod->num_objects == mod->objects_capacity) {
        size_t next = mod->objects_capacity ? mod->objects_capacity * 2 : 8;
        filc_ptr* next_refs = (filc_ptr*)realloc(mod->object_refs, next * sizeof(filc_ptr));
        if (!next_refs)
            filc_safety_panic(NULL, "out of memory tracking JIT objects.");
        mod->object_refs = next_refs;
        mod->objects_capacity = next;
    }
    mod->object_refs[mod->num_objects++] = ref;
    /* The marker reads this array without holding mod->lock, so publish the
       reference with a barrier just like push_root_locked does. Otherwise a
       concurrent marking that already scanned this module would miss the new
       edge and sweep a live GC section object. */
    if (filc_ptr_is_markable(ref))
        filc_store_barrier(my_thread, filc_ptr_object(ref));
}

static void push_root_locked(filc_thread* my_thread,
                             filc_llvm_jit_jit_module_object* mod, filc_ptr ref)
{
    PAS_UNUSED_PARAM(my_thread);
    pas_lock_lock(&mod->lock);
    if (mod->num_roots == mod->roots_capacity) {
        size_t next = mod->roots_capacity ? mod->roots_capacity * 2 : 4;
        filc_ptr* next_roots = (filc_ptr*)realloc(mod->roots, next * sizeof(filc_ptr));
        if (!next_roots) {
            pas_lock_unlock(&mod->lock);
            filc_safety_panic(NULL, "out of memory recording JIT resolve root.");
        }
        mod->roots = next_roots;
        mod->roots_capacity = next;
    }
    mod->roots[mod->num_roots++] = ref;
    pas_lock_unlock(&mod->lock);
    if (filc_ptr_is_markable(ref))
        filc_store_barrier(my_thread, filc_ptr_object(ref));
}

static void* allocate_jit_module_cb(void* context,
                                     unsigned long long combined_aux_size,
                                     unsigned long long other_sections_size)
{
    /* context is a callback token, not a pointer: decode and validate it.
       A dead or unknown token means the module is gone; refuse the
       allocation so the link fails cleanly instead of touching freed
       memory. The decoded module is rooted for the duration of the call.
       The thread gate comes first: anchor decode roots the module (GC
       state), which is only legal on the compiling entered Fil-C thread. */
    filc_thread* my_thread =
        jit_callback_check_thread(context, true, "allocate_jit_module");
    if (!my_thread)
        return NULL;
    filc_llvm_jit_jit_module_object* mod = jit_anchor_acquire(my_thread, context);
    if (!mod)
        return NULL;
    /* Claim the single allowed allocate with an atomic exchange. ORC may
       run this callback on any thread, so a plain bool load/store would
       race; the exchange claims exactly once. A second allocate means
       something linked lazily after CompileModule returned: record the
       error for the CompileModule thread (which may be a different thread
       than this one) and fail the link cleanly (NULL) so CompileModule
       fails instead of asserting or aborting. */
    if (__atomic_exchange_n(&mod->link_did_allocate, true, __ATOMIC_ACQ_REL)) {
        jit_callback_error(context,
                           "FilLLVMJIT_CompileModule: unexpected second JITLink "
                           "allocate for module; lazy linking after return is not supported.");
        jit_anchor_release(my_thread, context);
        return NULL;
    }
    size_t combined = (size_t)combined_aux_size;
    size_t other = (size_t)other_sections_size;
    size_t tail_size = combined + other;
    if (tail_size) {
        mod->tail = (char*)malloc(tail_size);
        if (!mod->tail) {
            jit_anchor_release(my_thread, context);
            return NULL;
        }
        memset(mod->tail, 0, tail_size);
    } else {
        mod->tail = NULL;
    }
    mod->combined_aux_size = combined;
    mod->combined_aux_used = 0;
    mod->other_size = other;
    mod->other_used = 0;
    /* The LinkGraph keeps this token opaquely and passes it back to every
       later callback, so it stays valid only while the anchor is alive;
       teardown marks the anchor dead first, turning any later use into a
       clean NULL. */
    void* token = context;
    jit_anchor_release(my_thread, context);
    return token;
}

static void* jit_alloc_with_backing(size_t total)
{
    void* result = jit_heap_try_allocate(total);
    if (result)
        return result;
    size_t chunk = (size_t)(64u << 20);
    if (total > chunk) {
        size_t unit = (size_t)(16u << 20);
        chunk = (total + unit - 1) & ~(unit - 1);
    }
    void* mem = mmap(NULL, chunk, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED)
        return NULL;
    jit_heap_add_fresh_memory(pas_range_create((uintptr_t)mem, (uintptr_t)mem + chunk));
    return jit_heap_try_allocate(total);
}

static void* allocate_executable_cb(void* context, void* jit_module,
                                     unsigned long long size,
                                     unsigned long long alignment,
                                     const char* section_name)
{
    PAS_UNUSED_PARAM(context);
    PAS_UNUSED_PARAM(section_name);
    if (!size)
        return NULL;
    /* jit_module is a callback token: decode it. A dead token refuses the
       allocation so the link fails cleanly. The thread gate comes first:
       the GC allocation below is only legal on the compiling entered
       Fil-C thread. */
    filc_thread* my_thread =
        jit_callback_check_thread(jit_module, true, "allocate_executable");
    if (!my_thread)
        return NULL;
    filc_llvm_jit_jit_module_object* mod = jit_anchor_acquire(my_thread, jit_module);
    if (!mod)
        return NULL;
    size_t request = (size_t)size;
    size_t align = (size_t)alignment;
    if (align < 16)
        align = 16;
    size_t total = request + align;
    if (total < request) {
        jit_anchor_release(my_thread, jit_module);
        return NULL;
    }
    void* raw = jit_alloc_with_backing(total);
    if (!raw) {
        jit_anchor_release(my_thread, jit_module);
        return NULL;
    }
    uintptr_t raw_addr = (uintptr_t)raw;
    uintptr_t aligned = (raw_addr + (align - 1)) & ~(uintptr_t)(align - 1);
    push_jit_base(mod, raw);
    jit_anchor_release(my_thread, jit_module);
    return (void*)aligned;
}

static void* allocate_object_cb(void* context, void* jit_module,
                                 const char* section_name,
                                 unsigned long long size,
                                 unsigned long long alignment)
{
    PAS_UNUSED_PARAM(context);
    PAS_UNUSED_PARAM(section_name);
    if (!size)
        return NULL;
    filc_thread* my_thread =
        jit_callback_check_thread(jit_module, true, "allocate_object");
    if (!my_thread)
        return NULL;
    filc_llvm_jit_jit_module_object* mod = jit_anchor_acquire(my_thread, jit_module);
    if (!mod)
        return NULL;
    size_t request = (size_t)size;
    size_t align = (size_t)alignment;
    if (align < 32)
        align = 32;
    filc_object* object = filc_allocate_with_alignment(my_thread, request, align);
    if (!object) {
        jit_anchor_release(my_thread, jit_module);
        return NULL;
    }
    filc_ptr ref = filc_ptr_create_with_object(my_thread, object);
    push_object_ref(my_thread, mod, ref);
    void* result = filc_object_lower(object);
    jit_anchor_release(my_thread, jit_module);
    return result;
}

static void* allocate_module_data_cb(void* context, void* jit_module,
                                      const char* section_name,
                                      unsigned long long size,
                                      unsigned long long alignment)
{
    PAS_UNUSED_PARAM(context);
    if (!size)
        return NULL;
    filc_thread* my_thread =
        jit_callback_check_thread(jit_module, true, "allocate_module_data");
    if (!my_thread)
        return NULL;
    filc_llvm_jit_jit_module_object* mod = jit_anchor_acquire(my_thread, jit_module);
    if (!mod)
        return NULL;
    size_t request = (size_t)size;
    size_t align = (size_t)alignment;
    if (align < 1)
        align = 1;
    bool is_aux = section_name
        && !strncmp(section_name, FILC_LLVM_JIT_AUX_SECTION_PREFIX,
                    strlen(FILC_LLVM_JIT_AUX_SECTION_PREFIX));
    char* tail = mod->tail;
    void* result = NULL;
    if (is_aux) {
        size_t off = (mod->combined_aux_used + (align - 1)) & ~(align - 1);
        if (off + request > mod->combined_aux_size) {
            jit_anchor_release(my_thread, jit_module);
            return NULL;
        }
        result = tail + off;
        mod->combined_aux_used = off + request;
    } else {
        size_t base = mod->combined_aux_size;
        size_t off = (mod->other_used + (align - 1)) & ~(align - 1);
        if (off + request > mod->other_size) {
            jit_anchor_release(my_thread, jit_module);
            return NULL;
        }
        result = tail + base + off;
        mod->other_used = off + request;
    }
    jit_anchor_release(my_thread, jit_module);
    return result;
}

static void finalize_object_cb(void* context, void* jit_module, void* object,
                                unsigned long long size)
{
    PAS_UNUSED_PARAM(context);
    PAS_UNUSED_PARAM(size);
    if (!jit_module || !object)
        return;
    /* Thread gate first: stamping the header touches GC state and is only
       legal on the compiling entered Fil-C thread. On failure just return;
       the link already failed (or never started) without this finalize. */
    filc_thread* my_thread =
        jit_callback_check_thread(jit_module, true, "finalize_object");
    if (!my_thread)
        return;
    filc_llvm_jit_jit_module_object* mod = jit_anchor_acquire(my_thread, jit_module);
    if (!mod)
        return;
    filc_object* owner = filc_object_for_lower(object);
    if (!owner) {
        jit_anchor_release(my_thread, jit_module);
        return;
    }
    if (!filc_object_is_aligned(owner)) {
        jit_anchor_release(my_thread, jit_module);
        return;
    }
    filc_alignment_header* header = filc_object_get_alignment_header(owner);
    filc_object* jit_object = filc_object_for_special_payload(mod);
    header->extra_pointer =
        ((uintptr_t)jit_object & ~FILC_EXTRA_POINTER_STATE_MASK)
        | FILC_EXTRA_POINTER_STATE_JIT_MODULE;
    jit_anchor_release(my_thread, jit_module);
}

static void* resolve_impl(filc_thread* my_thread,
                          filc_llvm_jit_jit_module_object* mod,
                          const char* name,
                          filc_ptr* resolved_out)
{
    if (resolved_out)
        *resolved_out = filc_ptr_forge_null();
    if (!name || !mod)
        return NULL;
    filc_ptr resolver;
    filc_ptr resolver_context;
    pas_lock_lock(&mod->lock);
    resolver = mod->resolver;
    resolver_context = mod->resolver_context;
    pas_lock_unlock(&mod->lock);
    if (filc_ptr_is_totally_null(resolver))
        return NULL;
    filc_ptr name_string = filc_strdup(my_thread, name);
    extern filc_ptr filc_call_user_ptr_ptr_ptr(filc_thread* my_thread, filc_ptr target,
                                               filc_ptr arg0, filc_ptr arg1);
    filc_ptr resolved = filc_call_user_ptr_ptr_ptr(my_thread, resolver, name_string,
                                                   resolver_context);
    if (filc_ptr_is_totally_null(resolved))
        return NULL;
    void* address = filc_ptr_ptr(resolved);
    push_root_locked(my_thread, mod, resolved);
    if (resolved_out)
        *resolved_out = resolved;
    return address;
}

static void* resolve_raw_impl(filc_thread* my_thread,
                              filc_llvm_jit_jit_module_object* mod,
                              filc_exact_ptr_table* table,
                              unsigned long long encoded,
                              filc_ptr* resolved_out)
{
    if (resolved_out)
        *resolved_out = filc_ptr_forge_null();
    filc_ptr resolved;
    if (table) {
        resolved = filc_exact_ptr_table_decode(my_thread, table, (uintptr_t)encoded);
    } else {
        if (!mod)
            return NULL;
        filc_ptr table_ref;
        pas_lock_lock(&mod->lock);
        table_ref = mod->zexact_table;
        pas_lock_unlock(&mod->lock);
        if (filc_ptr_is_totally_null(table_ref))
            return (void*)(uintptr_t)encoded;
        filc_check_access_special(table_ref, FILC_SPECIAL_TYPE_EXACT_PTR_TABLE);
        table = (filc_exact_ptr_table*)filc_ptr_ptr(table_ref);
        resolved = filc_exact_ptr_table_decode(my_thread, table, (uintptr_t)encoded);
    }
    if (filc_ptr_is_totally_null(resolved))
        return NULL;
    void* address = filc_ptr_ptr(resolved);
    if (mod)
        push_root_locked(my_thread, mod, resolved);
    if (resolved_out)
        *resolved_out = resolved;
    return address;
}

static void* resolve_symbol_cb(void* context, void* jit_module,
                                 const char* name)
{
    PAS_UNUSED_PARAM(context);
    if (!jit_module || !name)
        return NULL;
    /* Unlike the allocate path, this callback is baked into JITed code and
       may fire on any thread running that code, long after CompileModule
       returned — so any entered Fil-C thread is acceptable, not just the
       compiling one. Unknown symbols are a normal NULL (not an error). */
    filc_thread* my_thread =
        jit_callback_check_thread(jit_module, false, "resolve_symbol");
    if (!my_thread)
        return NULL;
    filc_llvm_jit_jit_module_object* mod = jit_anchor_acquire(my_thread, jit_module);
    if (!mod)
        return NULL;
    void* result = resolve_impl(my_thread, mod, name, NULL);
    jit_anchor_release(my_thread, jit_module);
    return result;
}

static void* resolve_raw_pointer_cb(void* context, void* jit_module,
                                     unsigned long long encoded)
{
    PAS_UNUSED_PARAM(context);
    if (!jit_module)
        return (void*)(uintptr_t)encoded;
    /* Same threading contract as resolve_symbol_cb: any entered Fil-C
       thread may run JITed code. On a thread violation return NULL (fail
       cleanly) rather than the raw encoding, which must only be handed out
       with the root that resolve_raw_impl records. */
    filc_thread* my_thread =
        jit_callback_check_thread(jit_module, false, "resolve_raw_pointer");
    if (!my_thread)
        return NULL;
    filc_llvm_jit_jit_module_object* mod = jit_anchor_acquire(my_thread, jit_module);
    if (!mod)
        return NULL;
    void* result = resolve_raw_impl(my_thread, mod, NULL, encoded, NULL);
    jit_anchor_release(my_thread, jit_module);
    return result;
}

void* filc_llvm_jit_resolve(void* jit_module, const char* name)
{
    PAS_UNUSED_PARAM(jit_module);
    PAS_UNUSED_PARAM(name);
    return NULL;
}

void* filc_llvm_jit_resolve_raw_pointer(void* jit_module, unsigned long long encoded)
{
    PAS_UNUSED_PARAM(jit_module);
    PAS_UNUSED_PARAM(encoded);
    return NULL;
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

filc_ptr filc_native_FilLLVMJIT_GetLastError(filc_thread* my_thread)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    /* Prefer the runtime-side sticky error: it covers failures that
       originate on the runtime side of CompileModule (no linkable
       definitions, unsupported linkage, eager-link failures after the
       shim already reported), for which the shim set nothing. */
    const char* sticky = jit_sticky_error_get();
    if (sticky)
        return filc_strdup(my_thread, sticky);
    const char* error = api->GetLastError ? api->GetLastError() : "";
    if (!error)
        error = "";
    return filc_strdup(my_thread, error);
}

filc_ptr filc_native_FilLLVMJIT_Create(filc_thread* my_thread)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    ShimJit handle = api->Create();
    jit_register_teardown_hook();
    if (!handle)
        return filc_ptr_forge_null();
    filc_object* object = filc_allocate_special_with_heap(
        my_thread, sizeof(filc_llvm_jit_session_object), 16,
        FILC_SPECIAL_TYPE_LLVM_JIT, fugc_destructor_heap);
    filc_llvm_jit_session_object* session =
        (filc_llvm_jit_session_object*)filc_object_lower(object);
    session->kind = FILC_LLVM_JIT_KIND_JIT_SESSION;
    pas_lock_construct(&session->lock);
    session->handle = (void*)handle;
    session->resolver = filc_ptr_forge_null();
    session->resolver_context = filc_ptr_forge_null();
    session->zexact_table = filc_ptr_forge_null();
    return filc_ptr_create_with_special_object(my_thread, object);
}

int filc_native_FilLLVMJIT_SetOption(filc_thread* my_thread, filc_ptr jit_ptr,
                                     filc_ptr key_ptr, filc_ptr value_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_session_object* session = unwrap_session(my_thread, jit_ptr);
    char* key = filc_check_and_get_tmp_str(my_thread, key_ptr);
    char* value = filc_check_and_get_tmp_str(my_thread, value_ptr);
    void* session_handle;
    pas_lock_lock(&session->lock);
    session_handle = session->handle;
    pas_lock_unlock(&session->lock);
    return api->SetOption((ShimJit)session_handle, key, value);
}

void filc_native_FilLLVMJIT_SetResolveCallback(filc_thread* my_thread, filc_ptr jit_ptr,
                                               filc_ptr resolver_ptr, filc_ptr context_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_session_object* session = unwrap_session(my_thread, jit_ptr);
    FILC_CHECK(!filc_ptr_is_totally_null(resolver_ptr), NULL,
               "FilLLVMJIT_SetResolveCallback: null resolver.");
    filc_check_function_call(resolver_ptr);
    pas_lock_lock(&session->lock);
    session->resolver = resolver_ptr;
    session->resolver_context = context_ptr;
    pas_lock_unlock(&session->lock);
    filc_store_barrier(my_thread, filc_ptr_object(resolver_ptr));
    if (filc_ptr_is_markable(context_ptr))
        filc_store_barrier(my_thread, filc_ptr_object(context_ptr));
}

int filc_native_FilLLVMJIT_SetZExactPointerTable(filc_thread* my_thread, filc_ptr jit_ptr,
                                                 filc_ptr table_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_session_object* session = unwrap_session(my_thread, jit_ptr);
    FILC_CHECK(!filc_ptr_is_totally_null(table_ptr), NULL,
               "FilLLVMJIT_SetZExactPointerTable: null table.");
    filc_check_access_special(table_ptr, FILC_SPECIAL_TYPE_EXACT_PTR_TABLE);
    pas_lock_lock(&session->lock);
    session->zexact_table = table_ptr;
    pas_lock_unlock(&session->lock);
    filc_store_barrier(my_thread, filc_ptr_object(table_ptr));
    return 0;
}

int filc_native_FilLLVMJIT_AddGlobalMapping(filc_thread* my_thread, filc_ptr jit_ptr,
                                            filc_ptr name_ptr, filc_ptr address_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_session_object* session = unwrap_session(my_thread, jit_ptr);
    char* name = filc_check_and_get_tmp_str(my_thread, name_ptr);
    void* address;
    if (filc_ptr_is_boxed_int(address_ptr)) {
        address = filc_ptr_ptr(address_ptr);
        FILC_CHECK((uintptr_t)address >= 4096, NULL,
                   "FilLLVMJIT_AddGlobalMapping: refusing to map implausible host address.");
    } else {
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
                   "FilLLVMJIT_AddGlobalMapping: address outside capability.");
    }
    void* session_handle;
    pas_lock_lock(&session->lock);
    session_handle = session->handle;
    pas_lock_unlock(&session->lock);
    return api->AddGlobalMapping((ShimJit)session_handle, name, address);
}

// Current Fil-C origin for getter calls is the calling frame origin.
// Native thunks push a frame before reaching here, so top_frame names the
// call site. A missing origin is a clean lookup failure (NULL + sticky
// error, which FilLLVMJIT_GetLastError surfaces), never a panic: the
// runtime has no shim-side SetLastError, so the sticky buffer is the
// error channel here, exactly as for CompileModule's runtime-side
// failures.
static const filc_origin* lookup_origin(filc_thread* my_thread)
{
    if (my_thread && my_thread->top_frame && my_thread->top_frame->origin)
        return my_thread->top_frame->origin;
    jit_set_last_error("JIT lookup: no caller origin available; refusing to invoke "
                       "pizlonated getter with a null origin.");
    return NULL;
}

static filc_ptr lookup_address(filc_thread* my_thread, filc_ptr module_ptr,
                               filc_ptr name_ptr,
                               const struct filc_llvm_jit_api* api,
                               void* (*lookup)(ShimJitModule, const char*))
{
    ensure_api(my_thread);
    jit_clear_last_error();
    filc_llvm_jit_jit_module_object* mod = unwrap_jit_module(my_thread, module_ptr);
    FILC_CHECK(mod, NULL, "JIT lookup: null module.");
    char* name = filc_check_and_get_tmp_str(my_thread, name_ptr);
    void* mod_handle;
    pas_lock_lock(&mod->lock);
    mod_handle = mod->handle;
    pas_lock_unlock(&mod->lock);
    // Lock covers only the handle load. The shim lookup below may link and
    // run the allocate callbacks, which need the thread entered and must not
    // block on mod->lock; holding no lock here proves no deadlock and no
    // alloc runs while the lock is held. Staying entered is required because
    // the allocate path calls filc_allocate.
    //
    // A missing origin fails cleanly (sticky set above) before any
    // pollcheck or getter call, so the getter below never sees NULL and no
    // safepoint is skipped.
    const filc_origin* origin = lookup_origin(my_thread);
    if (!origin)
        return filc_ptr_forge_null();
    filc_pollcheck(my_thread, origin);
    void* address = lookup((ShimJitModule)mod_handle, name);
    if (!address) {
        // Report the cause instead of swallowing it: the caller cannot
        // otherwise distinguish "unknown symbol" from a link failure, and
        // FilLLVMJIT_GetLastError prefers this sticky over the shim string.
        // The shim lookup runs synchronously on this thread, so reading
        // the shim's thread-local error here is sound.
        const char* shim_err = (api && api->GetLastError) ? api->GetLastError() : NULL;
        if (!shim_err || !shim_err[0])
            shim_err = "unknown error";
        jit_set_last_error("JIT lookup: failed to resolve '%s': %s.", name, shim_err);
        return filc_ptr_forge_null();
    }
    pizlonated_getter getter = (pizlonated_getter)address;
    // Pass the real caller origin so any panic inside the getter reports the
    // JIT lookup call site. The getter needs no catching frame: getters
    // materialize an already-linked global and run no user resolver (user
    // resolvers execute only inside resolve_impl, reached from JITed code
    // or the FilLLVMJIT_Resolve probe, never from an address-lookup
    // getter), so there is no user path that could throw a catchable
    // exception here; any unwind propagates to the Fil-C caller through the
    // native thunk frames as for other runtime getters. Pollchecks bracket
    // the call with the non-null origin above.
    filc_ptr result = getter(my_thread, origin);
    if (filc_ptr_is_totally_null(result)) {
        jit_set_last_error("JIT lookup: getter returned null for '%s'.", name);
        return filc_ptr_forge_null();
    }
    filc_pollcheck(my_thread, origin);
    return result;
}

filc_ptr filc_native_FilLLVMJIT_GetFunctionAddress(filc_thread* my_thread, filc_ptr module_ptr,
                                                   filc_ptr name_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    // All three lookups invoke the pizlonated getter so data globals keep
    // their capability token; returning the raw shim address would bypass it.
    return lookup_address(my_thread, module_ptr, name_ptr, api, api->GetFunctionAddress);
}

filc_ptr filc_native_FilLLVMJIT_GetGlobalValueAddress(filc_thread* my_thread,
                                                      filc_ptr module_ptr, filc_ptr name_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    return lookup_address(my_thread, module_ptr, name_ptr, api, api->GetGlobalValueAddress);
}

filc_ptr filc_native_FilLLVMJIT_GetPointerToGlobal(filc_thread* my_thread, filc_ptr module_ptr,
                                                   filc_ptr name_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    return lookup_address(my_thread, module_ptr, name_ptr, api, api->GetPointerToGlobal);
}

/* Classify one module-level definition for eager linking: 1 means link
   it, 0 means skip it, -1 means fail the compile (sticky error is set).
   Declarations and available-externally definitions emit no code and are
   skipped: resolving them would fail the compile spuriously. Weak and
   linkonce linkages have merging semantics the eager single-allocate model
   cannot support; leaving them for lazy linking would allocate outside
   CompileModule and fail the link on the link_did_allocate guard, so they fail the
   compile with a clean error instead. Aliases and ifuncs are linkable
   definitions exactly like functions and globals. */
static int eager_classify(const struct filc_llvm_jit_api* api, LLVMValueRef value)
{
    LLVMLinkage linkage;
    if (api->LLVMIsDeclaration(value))
        return 0;
    linkage = api->LLVMGetLinkage(value);
    if (linkage == LLVMAvailableExternallyLinkage)
        return 0;
    switch (linkage) {
    case LLVMExternalLinkage:
    case LLVMDLLExportLinkage:
    case LLVMCommonLinkage:
        return 1;
    case LLVMLinkOnceAnyLinkage:
    case LLVMLinkOnceODRLinkage:
    case LLVMLinkOnceODRAutoHideLinkage:
    case LLVMWeakAnyLinkage:
    case LLVMWeakODRLinkage:
    case LLVMExternalWeakLinkage:
    case LLVMLinkerPrivateWeakLinkage: {
        const char* sym_name = api->LLVMGetValueName(value);
        if (!sym_name || !sym_name[0])
            sym_name = "<unnamed>";
        jit_set_last_error("FilLLVMJIT_CompileModule: unsupported weak/linkonce linkage "
                           "for '%s'.",
                           sym_name);
        return -1;
    }
    default:
        return 0;
    }
}

filc_ptr filc_native_FilLLVMJIT_CompileModule(filc_thread* my_thread, filc_ptr jit_ptr,
                                              filc_ptr module_ptr, filc_ptr name_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_session_object* session = unwrap_session(my_thread, jit_ptr);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module_as_donor = NULL;
    filc_llvm_jit_object* module_obj =
        unwrap_obj(my_thread, module_ptr, FILC_LLVM_JIT_KIND_MODULE,
                   &context, &module_as_donor);
    FILC_CHECK(module_obj, NULL, "FilLLVMJIT_CompileModule: null module.");
    char* name = filc_check_and_get_tmp_str_or_null(my_thread, name_ptr);
    void* session_handle;
    void* module_handle = module_obj->handle;
    filc_llvm_jit_context_state* context_state = context->state;
    pas_lock_lock(&session->lock);
    session_handle = session->handle;
    pas_lock_unlock(&session->lock);
    filc_object* jit_object = filc_allocate_special_with_heap(
        my_thread, sizeof(filc_llvm_jit_jit_module_object), 16,
        FILC_SPECIAL_TYPE_LLVM_JIT, fugc_destructor_heap);
    filc_llvm_jit_jit_module_object* created =
        (filc_llvm_jit_jit_module_object*)filc_object_lower(jit_object);
    created->kind = FILC_LLVM_JIT_KIND_JIT_MODULE;
    pas_lock_construct(&created->lock);
    created->handle = NULL;
    created->jit_bases = NULL;
    created->num_jit_bases = 0;
    created->jit_bases_capacity = 0;
    created->object_refs = NULL;
    created->num_objects = 0;
    created->objects_capacity = 0;
    created->roots = NULL;
    created->num_roots = 0;
    created->roots_capacity = 0;
    pas_lock_lock(&session->lock);
    created->resolver = session->resolver;
    created->resolver_context = session->resolver_context;
    created->zexact_table = session->zexact_table;
    pas_lock_unlock(&session->lock);
    created->combined_aux_size = 0;
    created->combined_aux_used = 0;
    created->other_size = 0;
    created->other_used = 0;
    created->tail = NULL;
    /* __atomic access only (see filc_runtime.h): the allocate callback may
       run on any ORC thread. */
    __atomic_store_n(&created->link_did_allocate, false, __ATOMIC_RELAXED);
    filc_ptr jit_keep = filc_ptr_create_with_special_object(my_thread, jit_object);
    /* Root the new module object for the duration of the compile. Until we
       return jit_keep to the caller, the only references are this C local
       and the shim's snapshots, neither of which the collector scans. The
       thread exits below (handshake) and the shim links lazily on lookup,
       so without this root a concurrent collection could sweep the module
       out from under us mid-compile. Popped before each return, once the
       flight is published to the caller (or on failure paths). */
    filc_push_allocation_root(my_thread, jit_object);
    jit_clear_last_error();
    /* Allocate the callback token before installing the callbacks. The shim
       only ever sees this token, never the raw payload pointer: it is baked
       into the dylib snapshot, the filc_jit_module absolute symbol, and the
       module-private resolve wrappers. Decoding validates liveness, so a
       stale token fails cleanly instead of dangling. */
    void* token = jit_anchor_alloc(jit_object);
    if (!token) {
        jit_set_last_error("FilLLVMJIT_CompileModule: out of memory allocating callback token.");
        filc_pop_allocation_root(my_thread, jit_object);
        return filc_ptr_forge_null();
    }
    /* Register the compiling thread for this token before any shim call
       can run a callback: the allocate-path callbacks verify they run on
       this thread, and report cross-thread failures into the token's
       locked slot for the drains below to pick up. */
    jit_compile_record_add(token, my_thread);
    struct filc_llvm_jit_memory_callbacks cbs;
    cbs.context = token;
    cbs.allocate_jit_module = allocate_jit_module_cb;
    cbs.allocate_executable = allocate_executable_cb;
    cbs.allocate_object = allocate_object_cb;
    cbs.allocate_module_data = allocate_module_data_cb;
    cbs.finalize_object = finalize_object_cb;
    cbs.resolve_symbol = resolve_symbol_cb;
    cbs.resolve_raw_pointer = resolve_raw_pointer_cb;
    /* SetMemoryCallbacks is a link-phase shim call like CompileModule
       below: run it through the teardown gate (lock-free try-enter/leave,
       never holding any lock across it) so a compile racing process exit
       fails cleanly instead of entering the shim during static
       destruction. */
    bool cbs_installed = false;
    bool cbs_skipped_at_exit = false;
    if (jit_teardown_try_enter()) {
        cbs_installed = !api->SetMemoryCallbacks((ShimJit)session_handle, &cbs);
        jit_teardown_leave();
    } else {
        cbs_skipped_at_exit = true;
    }
    if (!cbs_installed) {
        if (cbs_skipped_at_exit) {
            jit_set_last_error("FilLLVMJIT_CompileModule: SetMemoryCallbacks skipped: "
                               "process is exiting.");
        } else {
            const char* shim_err = api->GetLastError ? api->GetLastError() : NULL;
            if (!shim_err || !shim_err[0])
                shim_err = "unknown error";
            jit_set_last_error("FilLLVMJIT_CompileModule: SetMemoryCallbacks failed: %s.",
                               shim_err);
        }
        return jit_compile_fail(my_thread, jit_object, token, NULL, NULL, 0);
    }
    struct eager_symbol* eager = NULL;
    size_t num_eager = 0;
    size_t cap_eager = 0;
    bool scan_failed = false;
    pas_lock_lock(&context_state->lock);
    {
        LLVMValueRef value;
        for (value = api->LLVMGetFirstFunction((LLVMModuleRef)module_handle);
             value && !scan_failed;
             value = api->LLVMGetNextFunction(value)) {
            int verdict = eager_classify(api, value);
            if (verdict < 0)
                scan_failed = true;
            else if (verdict > 0) {
                const char* sym_name = api->LLVMGetValueName(value);
                if (sym_name && sym_name[0])
                    push_eager_symbol(&eager, &num_eager, &cap_eager,
                                      sym_name, true);
            }
        }
        for (value = api->LLVMGetFirstGlobal((LLVMModuleRef)module_handle);
             value && !scan_failed;
             value = api->LLVMGetNextGlobal(value)) {
            int verdict = eager_classify(api, value);
            if (verdict < 0)
                scan_failed = true;
            else if (verdict > 0) {
                const char* sym_name = api->LLVMGetValueName(value);
                if (sym_name && sym_name[0])
                    push_eager_symbol(&eager, &num_eager, &cap_eager,
                                      sym_name, false);
            }
        }
        for (value = api->LLVMGetFirstGlobalAlias((LLVMModuleRef)module_handle);
             value && !scan_failed;
             value = api->LLVMGetNextGlobalAlias(value)) {
            int verdict = eager_classify(api, value);
            if (verdict < 0)
                scan_failed = true;
            else if (verdict > 0) {
                const char* sym_name = api->LLVMGetValueName(value);
                if (sym_name && sym_name[0])
                    push_eager_symbol(&eager, &num_eager, &cap_eager,
                                      sym_name, false);
            }
        }
        for (value = api->LLVMGetFirstGlobalIFunc((LLVMModuleRef)module_handle);
             value && !scan_failed;
             value = api->LLVMGetNextGlobalIFunc(value)) {
            int verdict = eager_classify(api, value);
            if (verdict < 0)
                scan_failed = true;
            else if (verdict > 0) {
                const char* sym_name = api->LLVMGetValueName(value);
                if (sym_name && sym_name[0])
                    push_eager_symbol(&eager, &num_eager, &cap_eager,
                                      sym_name, true);
            }
        }
    }
    if (scan_failed) {
        pas_lock_unlock(&context_state->lock);
        return jit_compile_fail(my_thread, jit_object, token, NULL, eager, num_eager);
    }
    /* Bug #3: the context lock covers only the scan above, which copies
       every link decision out into the malloc'd eager list (names plus
       function/global flag). It is released before any linking: holding it
       across CompileModule would hold it across the allocate callbacks
       (which run synchronously inside CompileModule and call
       filc_allocate, take other locks, and may trigger a collection), as
       well as across the eager lookups and the GC allocation below. Link
       unlocked; re-lock only to publish the handle on success. */
    pas_lock_unlock(&context_state->lock);
    /* CompileModule itself runs through the teardown gate (lock-free
       try-enter/leave, no lock held across it) so a compile racing
       process exit fails cleanly instead of entering the shim during
       static destruction. */
    ShimJitModule out = NULL;
    bool link_skipped_at_exit = false;
    if (jit_teardown_try_enter()) {
        out = api->CompileModule((ShimJit)session_handle,
                                 (LLVMModuleRef)module_handle,
                                 name ? name : "jit-module",
                                 token);
        jit_teardown_leave();
    } else {
        link_skipped_at_exit = true;
    }
    jit_register_teardown_hook();
    if (!out) {
        if (link_skipped_at_exit) {
            jit_set_last_error("FilLLVMJIT_CompileModule: CompileModule skipped: "
                               "process is exiting.");
        } else {
            const char* shim_err = api->GetLastError ? api->GetLastError() : NULL;
            if (!shim_err || !shim_err[0])
                shim_err = "unknown error";
            jit_set_last_error("FilLLVMJIT_CompileModule: CompileModule failed: %s.",
                               shim_err);
        }
        return jit_compile_fail(my_thread, jit_object, token, NULL, eager, num_eager);
    }
    /* Bug #9: nothing is published yet (created->handle stays NULL and the
       donor still owns its LLVM handles), so every failure below lands in
       jit_compile_fail with the half-linked shim module to destroy while
       the malloc'd link payload (jit heap bases, GC object refs, roots,
       tail) stays owned by the unpublished module object. The collector
       reclaims it through the destructor, which frees those arrays and
       — finding the abandoned anchor — skips the already-destroyed shim
       handle instead of destroying it twice. */
    if (!num_eager) {
        /* Nothing linkable was collected: returning the module would leave
           it permanently unlinked, and any later lookup would allocate
           outside CompileModule. Fail the compile with a clean error
           instead. */
        jit_set_last_error("FilLLVMJIT_CompileModule: module defines no linkable symbols.");
        return jit_compile_fail(my_thread, jit_object, token, (void*)out, eager, num_eager);
    }
    filc_ptr result = jit_keep;
    {
        // Eager link: resolve every externally visible definition collected
        // above (functions, globals, aliases, ifuncs) to force the single
        // LinkGraph allocate synchronously inside CompileModule, so nothing
        // remains to trigger a lazy allocate later (which would fail the
        // link on the link_did_allocate guard). These are raw LLJIT-level requests
        // only; getters run later in address lookups with the caller origin.
        // Every result is checked: a failure fails the compile cleanly
        // instead of limping on toward a later abort, and the handshake
        // below stays after this linking, so linked code is fenced before
        // any address can be called. Each lookup runs through the teardown
        // gate (lock-free try-enter/leave, no lock held across it) for the
        // same exit-race reason as CompileModule above.
        size_t index;
        bool eager_failed = false;
        for (index = 0; index < num_eager; ++index) {
            void* address = NULL;
            bool lookup_skipped_at_exit = false;
            if (jit_teardown_try_enter()) {
                if (eager[index].is_function)
                    address = api->GetFunctionAddress(out, eager[index].name);
                else
                    address = api->GetGlobalValueAddress(out, eager[index].name);
                jit_teardown_leave();
            } else {
                lookup_skipped_at_exit = true;
            }
            // A callback that failed on another thread recorded its cause
            // in the token's locked slot; surface it before the generic
            // shim string so the real cause wins over "unknown error".
            if (!address)
                jit_compile_record_drain(token);
            if (!address) {
                if (lookup_skipped_at_exit) {
                    jit_set_last_error("FilLLVMJIT_CompileModule: eager linking skipped "
                                       "for '%s': process is exiting.",
                                       eager[index].name);
                } else if (!jit_sticky_error_get()) {
                    const char* shim_err = api->GetLastError ? api->GetLastError() : NULL;
                    if (!shim_err || !shim_err[0])
                        shim_err = "unknown error";
                    jit_set_last_error("FilLLVMJIT_CompileModule: eager linking failed "
                                       "for '%s': %s.",
                                       eager[index].name, shim_err);
                }
                eager_failed = true;
                break;
            }
        }
        free_eager_symbols(eager, num_eager);
        eager = NULL;
        num_eager = 0;
        if (eager_failed)
            return jit_compile_fail(my_thread, jit_object, token, (void*)out, eager, num_eager);
    }
    /* Cross-modifying-code fence: thread-state contract (see filc_runtime.c:
       filc_soft_handshake asserts the caller is *not* entered, so the
       filc_exit above is mandatory, not optional). code_fence is
       thread-state-independent: it touches no Fil-C state and only issues a
       serializing instruction (cpuid/isb/fence), so it is correct both
       inside the handshake on every thread (entered threads run it at
       their next pollcheck, exited ones — including this caller — inline)
       and once more directly below for this thread's own pipeline while
       still exited. filc_enter then re-enters before the flight below is
       published to the caller. */
    filc_exit(my_thread);
    filc_soft_handshake(code_fence, NULL);
    code_fence(my_thread, NULL);
    filc_enter(my_thread);
    /* Publish the handle: take over the module and its dedicated context
       as is; no copy is made. The context object comes from the caller's
       LLVMContextCreate and is exclusively owned by this module, so unique
       ownership moves to the JIT here. This is the only locked section
       after the scan: linking (CompileModule, eager lookups) and the
       handshake all ran unlocked, so no callback ever ran while this lock
       was held. */
    pas_lock_lock(&context_state->lock);
    pas_lock_lock(&created->lock);
    created->handle = (void*)out;
    pas_lock_unlock(&created->lock);
    module_obj->handle = NULL;
    if (module_obj->module_state)
        module_obj->module_state->handle = NULL;
    module_as_donor = NULL;
    context_state->handle = NULL;
    pas_lock_unlock(&context_state->lock);
    jit_anchor_fill_shim(token, (void*)out);
    /* The result flight now roots the module from the caller's frame.
       Linking is done, so no callback can still need this thread check or
       error slot: drop the registry entry. Resolve callbacks baked into
       the JITed code keep working afterwards on any entered Fil-C thread
       via the anchor alone. */
    jit_compile_record_remove(token);
    filc_pop_allocation_root(my_thread, jit_object);
    return result;
}

filc_ptr filc_native_FilLLVMJIT_Resolve(filc_thread* my_thread, filc_ptr module_ptr,
                                        filc_ptr name_ptr)
{
    ensure_api(my_thread);
    filc_llvm_jit_jit_module_object* mod = unwrap_jit_module(my_thread, module_ptr);
    FILC_CHECK(mod, NULL, "FilLLVMJIT_Resolve: null module.");
    char* name = filc_check_and_get_tmp_str_or_null(my_thread, name_ptr);
    filc_ptr resolved = filc_ptr_forge_null();
    resolve_impl(my_thread, mod, name, &resolved);
    return resolved;
}

filc_ptr filc_native_FilLLVMJIT_ResolveRawPointer(filc_thread* my_thread, filc_ptr module_ptr,
                                                  filc_ptr table_ptr,
                                                  unsigned long long encoded)
{
    ensure_api(my_thread);
    filc_llvm_jit_jit_module_object* mod = unwrap_jit_module(my_thread, module_ptr);
    FILC_CHECK(mod, NULL, "FilLLVMJIT_ResolveRawPointer: null module.");
    filc_exact_ptr_table* table = NULL;
    if (!filc_ptr_is_totally_null(table_ptr)) {
        filc_check_access_special(table_ptr, FILC_SPECIAL_TYPE_EXACT_PTR_TABLE);
        table = (filc_exact_ptr_table*)filc_ptr_ptr(table_ptr);
    }
    filc_ptr resolved = filc_ptr_forge_null();
    resolve_raw_impl(my_thread, mod, table, encoded, &resolved);
    return resolved;
}

/* Hand-written natives for functions that the mechanical generator
   cannot express: out-arrays (with companion count queries), out-scalars,
   raw-word arrays, and multi-array calls. */

void filc_native_LLVMGetVersion(filc_thread* my_thread, filc_ptr major_ptr,
                                      filc_ptr minor_ptr, filc_ptr patch_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* value =
        unwrap_obj(my_thread, value_ptr, FILC_LLVM_JIT_KIND_VALUE,
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* value =
        unwrap_obj(my_thread, value_ptr, FILC_LLVM_JIT_KIND_VALUE,
                                 &context, &module);
    FILC_CHECK(value, NULL, "LLVMGlobalCopyAllMetadata: null value.");
    size_t num_entries = 0;
    LLVMValueMetadataEntry* entries = NULL;
    filc_check_write(num_entries_ptr, sizeof(size_t));
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    entries = api->LLVMGlobalCopyAllMetadata((LLVMValueRef)value->handle, &num_entries);
    filc_enter(my_thread);
    /* The entries array is never freed (the curated API has no Dispose for it); the
       wrapper records the count so the getters can bounds-check. */
    filc_ptr result = wrap_handle(my_thread, context, module,
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* entries =
        unwrap_obj(my_thread, entries_ptr,
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = unwrap_context(my_thread, context_ptr);
    filc_llvm_jit_object* module = NULL;
    char* name = filc_check_and_get_tmp_str(my_thread, name_ptr);
    LLVMTypeRef* type_params = NULL;
    unsigned* int_params = NULL;
    if (!filc_ptr_is_totally_null(type_params_ptr)) {
        void** decoded = decode_handle_array(
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
    filc_ptr result = wrap_handle(my_thread, context, NULL,
                                         FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    pas_lock_unlock(&context->state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstIntOfArbitraryPrecision(
    filc_thread* my_thread, filc_ptr int_ty_ptr, unsigned num_words, filc_ptr words_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* int_ty =
        unwrap_obj(my_thread, int_ty_ptr, FILC_LLVM_JIT_KIND_TYPE,
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
    filc_ptr result = wrap_handle(my_thread, context, module,
                                         FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    pas_lock_unlock(&context->state->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateConstantRangeAttribute(
    filc_thread* my_thread, filc_ptr context_ptr, unsigned kind_id, unsigned num_bits,
    filc_ptr lower_words_ptr, filc_ptr upper_words_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = unwrap_context(my_thread, context_ptr);
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
    filc_ptr result = wrap_handle(my_thread, context, NULL,
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    /* Operand bundles have no constructors in the curated API, so only null is
       accepted today (the count must be zero with it). */
    FILC_CHECK(filc_ptr_is_totally_null(bundles_ptr), NULL,
               "LLVMBuildCallBr: operand bundles are not supported in the curated API (pass null).");
    FILC_CHECK(!num_bundles, NULL,
               "LLVMBuildCallBr: nonzero bundle count with null bundles.");
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* builder =
        unwrap_obj(my_thread, builder_ptr, FILC_LLVM_JIT_KIND_BUILDER,
                                 &context, &module);
    FILC_CHECK(builder, NULL, "LLVMBuildCallBr: null builder.");
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
    const char* name = filc_check_and_get_tmp_str_or_null(my_thread, name_ptr);
    LLVMValueRef result_raw;
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCallBr((LLVMBuilderRef)builder->handle,
                                      (LLVMTypeRef)handle_of(ty),
                                      (LLVMValueRef)handle_of(fn),
                                      (LLVMBasicBlockRef)handle_of(default_dest),
                                      indirect_dests, num_indirect_dests,
                                      args, num_args, NULL, 0, name);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module,
                                         FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (module)
        builder_adopt_module(my_thread, builder, module);
    pas_lock_unlock(&context->state->lock);
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* fn = unwrap_obj(
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
    emit_out_array(my_thread, context, module, (void**)tmp, count, attrs_ptr,
                                 FILC_LLVM_JIT_KIND_ATTRIBUTE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetCallSiteAttributes(filc_thread* my_thread, filc_ptr call_ptr,
                                                 unsigned index, filc_ptr attrs_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* call = unwrap_obj(
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
    emit_out_array(my_thread, context, module, (void**)tmp, count, attrs_ptr,
                                 FILC_LLVM_JIT_KIND_ATTRIBUTE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetBasicBlocks(filc_thread* my_thread, filc_ptr fn_ptr,
                                          filc_ptr blocks_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* fn = unwrap_obj(
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
    emit_out_array(my_thread, context, module, (void**)tmp, count, blocks_ptr,
                                 FILC_LLVM_JIT_KIND_BASIC_BLOCK);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetHandlers(filc_thread* my_thread, filc_ptr catch_switch_ptr,
                                       filc_ptr handlers_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* catch_switch = unwrap_obj(
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
    emit_out_array(my_thread, context, module, (void**)tmp, count, handlers_ptr,
                                 FILC_LLVM_JIT_KIND_BASIC_BLOCK);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetMDNodeOperands(filc_thread* my_thread, filc_ptr value_ptr,
                                             filc_ptr dest_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* value = unwrap_obj(
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
    emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_VALUE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetNamedMetadataOperands(filc_thread* my_thread, filc_ptr module_ptr,
                                                    filc_ptr name_ptr, filc_ptr dest_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module_donor = NULL;
    filc_llvm_jit_object* module_obj = unwrap_obj(
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
    emit_out_array(my_thread, context, module_obj, (void**)tmp, count,
                                 dest_ptr, FILC_LLVM_JIT_KIND_VALUE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetParamTypes(filc_thread* my_thread, filc_ptr function_ty_ptr,
                                         filc_ptr dest_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* function_ty = unwrap_obj(
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
    emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_TYPE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetParams(filc_thread* my_thread, filc_ptr fn_ptr, filc_ptr dest_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* fn = unwrap_obj(
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
    emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_VALUE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetStructElementTypes(filc_thread* my_thread, filc_ptr struct_ty_ptr,
                                                 filc_ptr dest_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* struct_ty = unwrap_obj(
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
    emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_TYPE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

void filc_native_LLVMGetSubtypes(filc_thread* my_thread, filc_ptr ty_ptr, filc_ptr dest_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* ty = unwrap_obj(
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
    emit_out_array(my_thread, context, module, (void**)tmp, count, dest_ptr,
                                 FILC_LLVM_JIT_KIND_TYPE);
    free(tmp);
    pas_lock_unlock(&context->state->lock);
}

filc_ptr filc_native_LLVMContextCreate(filc_thread* my_thread)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
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
        const struct filc_llvm_jit_api* api = ensure_api(my_thread); \
        filc_llvm_jit_context_object* context = NULL; \
        filc_llvm_jit_object* module = NULL; \
        filc_llvm_jit_object* parent = unwrap_obj( \
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
        const struct filc_llvm_jit_api* api = ensure_api(my_thread); \
        filc_llvm_jit_context_object* context = NULL; \
        filc_llvm_jit_object* module = NULL; \
        filc_llvm_jit_object* parent = unwrap_obj( \
            my_thread, parent_ptr, kind, &context, &module); \
        len_type length = 0; \
        const char* raw; \
        FILC_CHECK(parent, NULL, #c_name ": null parent."); \
        pas_lock_lock(&context->state->lock); \
        filc_exit(my_thread); \
        raw = api->c_name((void*)parent->handle, &length); \
        filc_enter(my_thread); \
        filc_ptr result = \
            new_sized_string(my_thread, raw, (size_t)length); \
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    size_t length = 0;
    const char* raw;
    filc_exit(my_thread);
    raw = api->LLVMIntrinsicGetName(id, &length);
    filc_enter(my_thread);
    return new_sized_string(my_thread, raw, length);
}

filc_ptr filc_native_LLVMGetIndices(filc_thread* my_thread, filc_ptr inst_ptr)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* inst = unwrap_obj(
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* inst = unwrap_obj(
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
    filc_ptr result = wrap_handle(my_thread, context, module,
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    LLVMTypeRef* param_types = (LLVMTypeRef*)decode_handle_array(
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
       the curated API omits): the original leaks by design, only the copy escapes. */
    raw = api->LLVMIntrinsicCopyOverloadedName(id, param_types, param_count, &name_length);
    filc_enter(my_thread);
    filc_ptr result = new_sized_string(my_thread, raw, name_length);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIntrinsicCopyOverloadedName2(
    filc_thread* my_thread, filc_ptr module_ptr, unsigned id,
    filc_ptr param_types_ptr, size_t param_count)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module_donor = NULL;
    filc_llvm_jit_object* module_obj = unwrap_obj(
        my_thread, module_ptr, FILC_LLVM_JIT_KIND_MODULE, &context, &module_donor);
    LLVMTypeRef* param_types = (LLVMTypeRef*)decode_handle_array(
        my_thread, param_types_ptr, param_count, FILC_LLVM_JIT_KIND_TYPE,
        &context, &module_donor);
    size_t name_length = 0;
    char* raw;
    FILC_CHECK(module_obj, NULL, "LLVMIntrinsicCopyOverloadedName2: null module.");
    pas_lock_lock(&context->state->lock);
    filc_exit(my_thread);
    /* Owned by LLVM (would need LLVMDisposeMessage in the full C API, which
       the curated API omits): the original leaks by design, only the copy escapes. */
    raw = api->LLVMIntrinsicCopyOverloadedName2((LLVMModuleRef)module_obj->handle, id,
                                                param_types, param_count, &name_length);
    filc_enter(my_thread);
    filc_ptr result = new_sized_string(my_thread, raw, name_length);
    pas_lock_unlock(&context->state->lock);
    return result;
}

/* ------------------------------------------------------------------ */
/* Mechanically generated thin wrappers for the curated LLVM C API.
   Thin wrappers mechanically emitted for the curated LLVM C API;
   do not edit by hand. */
filc_ptr filc_native_LLVMAddAlias2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAddAlias2((LLVMModuleRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), p2, (LLVMValueRef)handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMAddAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddAttributeAtIndex((LLVMValueRef)handle_of(a0), p1, (LLVMAttributeRef)handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddCallSiteAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddCallSiteAttribute((LLVMValueRef)handle_of(a0), p1, (LLVMAttributeRef)handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddCase(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddCase((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMBasicBlockRef)handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddClause(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddClause((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddDestination(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddDestination((LLVMValueRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMAddFunction(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAddFunction((LLVMModuleRef)handle_of(a0), a1, (LLVMTypeRef)handle_of(a2));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMAddGlobal(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAddGlobal((LLVMModuleRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMAddGlobalIFunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1;
    if (filc_ptr_is_totally_null(p1)) {
        FILC_CHECK(p2 == 0, NULL, "LLVMAddGlobalIFunc: null string with nonzero length.");
        a1 = NULL;
    } else {
        filc_check_read(p1, (size_t)p2);
        a1 = (const char*)filc_ptr_ptr(p1);
    }
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a5 = unwrap_obj(my_thread, p5, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAddGlobalIFunc((LLVMModuleRef)handle_of(a0), a1, p2, (LLVMTypeRef)handle_of(a3), p4, (LLVMValueRef)handle_of(a5));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMAddGlobalInAddressSpace(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAddGlobalInAddressSpace((LLVMModuleRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), a2, p3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMAddHandler(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddHandler((LLVMValueRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddIncoming(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)decode_handle_array(my_thread, p1, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMBasicBlockRef* a2 = (LLVMBasicBlockRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddIncoming((LLVMValueRef)handle_of(a0), a1, a2, p3);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddMetadataToInst(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddMetadataToInst((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddModuleFlag(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, size_t p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a2;
    if (filc_ptr_is_totally_null(p2)) {
        FILC_CHECK(p3 == 0, NULL, "LLVMAddModuleFlag: null string with nonzero length.");
        a2 = NULL;
    } else {
        filc_check_read(p2, (size_t)p3);
        a2 = (const char*)filc_ptr_ptr(p2);
    }
    filc_llvm_jit_object* a4 = unwrap_obj(my_thread, p4, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddModuleFlag((LLVMModuleRef)handle_of(a0), p1, a2, p3, (LLVMMetadataRef)handle_of(a4));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddNamedMetadataOperand(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddNamedMetadataOperand((LLVMModuleRef)handle_of(a0), a1, (LLVMValueRef)handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAddTargetDependentFunctionAttr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAddTargetDependentFunctionAttr((LLVMValueRef)handle_of(a0), a1, a2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMAliasGetAliasee(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAliasGetAliasee((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMAliasSetAliasee(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAliasSetAliasee((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMAlignOf(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAlignOf((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMAppendBasicBlockInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMAppendBasicBlockInContext((LLVMContextRef)a0->state->handle, (LLVMValueRef)handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMAppendExistingBasicBlock(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMAppendExistingBasicBlock((LLVMValueRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMAppendModuleInlineAsm(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
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
    api->LLVMAppendModuleInlineAsm((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMArrayType(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMArrayType((LLVMTypeRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMArrayType2(filc_thread* my_thread, filc_ptr p0, unsigned long long p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMArrayType2((LLVMTypeRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMBFloatTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBFloatTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBasicBlockAsValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBasicBlockAsValue((LLVMBasicBlockRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBlockAddress(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBlockAddress((LLVMValueRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAShr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAShr((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAdd((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAddrSpaceCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAddrSpaceCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAggregateRet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAggregateRet((LLVMBuilderRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAlloca(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAlloca((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAnd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAnd((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildArrayAlloca(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildArrayAlloca((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildArrayMalloc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildArrayMalloc((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAtomicCmpXchg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, unsigned p5, bool p6)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAtomicCmpXchg((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), p4, p5, p6);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAtomicCmpXchgSyncScope(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, unsigned p5, unsigned p6)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAtomicCmpXchgSyncScope((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), p4, p5, p6);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAtomicRMW(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, unsigned p4, bool p5)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAtomicRMW((LLVMBuilderRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), p4, p5);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildAtomicRMWSyncScope(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, unsigned p4, unsigned p5)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildAtomicRMWSyncScope((LLVMBuilderRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), p4, p5);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildBinOp(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildBinOp((LLVMBuilderRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildBitCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildBr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildBr((LLVMBuilderRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCall2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a5 = filc_check_and_get_tmp_str_or_null(my_thread, p5);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCall2((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3, p4, a5);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCast(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCast((LLVMBuilderRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2), (LLVMTypeRef)handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCatchPad(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCatchPad((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2, p3, a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCatchRet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCatchRet((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMBasicBlockRef)handle_of(a2));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCatchSwitch(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCatchSwitch((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMBasicBlockRef)handle_of(a2), p3, a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCleanupPad(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCleanupPad((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2, p3, a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCleanupRet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCleanupRet((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMBasicBlockRef)handle_of(a2));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildCondBr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildCondBr((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMBasicBlockRef)handle_of(a2), (LLVMBasicBlockRef)handle_of(a3));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildExactSDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildExactSDiv((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildExactUDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildExactUDiv((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildExtractElement(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildExtractElement((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildExtractValue(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildExtractValue((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), p2, a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFAdd((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFCmp(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFCmp((LLVMBuilderRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFDiv((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFMul((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFNeg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFNeg((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFPCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPExt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFPExt((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPToSI(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFPToSI((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPToUI(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFPToUI((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFPTrunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFPTrunc((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFRem(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFRem((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFSub((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFence(filc_thread* my_thread, filc_ptr p0, unsigned p1, bool p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFence((LLVMBuilderRef)handle_of(a0), p1, p2, a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFenceSyncScope(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFenceSyncScope((LLVMBuilderRef)handle_of(a0), p1, p2, a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFree(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFree((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildFreeze(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildFreeze((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a5 = filc_check_and_get_tmp_str_or_null(my_thread, p5);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildGEP2((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3, p4, a5);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildGEPWithNoWrapFlags(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5, unsigned p6)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a5 = filc_check_and_get_tmp_str_or_null(my_thread, p5);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildGEPWithNoWrapFlags((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3, p4, a5, p6);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildGlobalString(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildGlobalString((LLVMBuilderRef)handle_of(a0), a1, a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildGlobalStringPtr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildGlobalStringPtr((LLVMBuilderRef)handle_of(a0), a1, a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildICmp(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildICmp((LLVMBuilderRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildInBoundsGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a5 = filc_check_and_get_tmp_str_or_null(my_thread, p5);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildInBoundsGEP2((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3, p4, a5);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIndirectBr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildIndirectBr((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildInsertElement(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildInsertElement((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildInsertValue(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildInsertValue((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), p3, a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIntCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildIntCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIntCast2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, bool p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildIntCast2((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), p3, a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIntToPtr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildIntToPtr((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildInvoke2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4, filc_ptr p5, filc_ptr p6, filc_ptr p7)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a3 = (LLVMValueRef*)decode_handle_array(my_thread, p3, p4, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a5 = unwrap_obj(my_thread, p5, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a6 = unwrap_obj(my_thread, p6, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    const char* a7 = filc_check_and_get_tmp_str_or_null(my_thread, p7);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildInvoke2((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3, p4, (LLVMBasicBlockRef)handle_of(a5), (LLVMBasicBlockRef)handle_of(a6), a7);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIsNotNull(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildIsNotNull((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildIsNull(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildIsNull((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildLShr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildLShr((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildLandingPad(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildLandingPad((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), p3, a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildLoad2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildLoad2((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMalloc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildMalloc((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMemCpy(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a5 = unwrap_obj(my_thread, p5, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildMemCpy((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), p2, (LLVMValueRef)handle_of(a3), p4, (LLVMValueRef)handle_of(a5));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMemMove(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3, unsigned p4, filc_ptr p5)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a5 = unwrap_obj(my_thread, p5, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildMemMove((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), p2, (LLVMValueRef)handle_of(a3), p4, (LLVMValueRef)handle_of(a5));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMemSet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, unsigned p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildMemSet((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), p4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildMul((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNSWAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNSWAdd((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNSWMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNSWMul((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNSWNeg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNSWNeg((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNSWSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNSWSub((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNUWAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNUWAdd((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNUWMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNUWMul((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNUWSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNUWSub((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNeg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNeg((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildNot(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildNot((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildOr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildOr((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildPhi(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildPhi((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildPointerCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildPointerCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildPtrDiff2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildPtrDiff2((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildPtrToInt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildPtrToInt((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildResume(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildResume((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildRet(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildRet((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildRetVoid(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildRetVoid((LLVMBuilderRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSDiv((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSExt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSExt((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSExtOrBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSExtOrBitCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSIToFP(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSIToFP((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSRem(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSRem((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSelect(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSelect((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildShl(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildShl((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildShuffleVector(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildShuffleVector((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3), a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildStore(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildStore((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildStructGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, filc_ptr p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a4 = filc_check_and_get_tmp_str_or_null(my_thread, p4);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildStructGEP2((LLVMBuilderRef)handle_of(a0), (LLVMTypeRef)handle_of(a1), (LLVMValueRef)handle_of(a2), p3, a4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSub((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildSwitch(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildSwitch((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMBasicBlockRef)handle_of(a2), p3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildTrunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildTrunc((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildTruncOrBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildTruncOrBitCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildUDiv(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildUDiv((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildUIToFP(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildUIToFP((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildURem(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildURem((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildUnreachable(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildUnreachable((LLVMBuilderRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildVAArg(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildVAArg((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildXor(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildXor((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildZExt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildZExt((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuildZExtOrBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a3 = filc_check_and_get_tmp_str_or_null(my_thread, p3);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuildZExtOrBitCast((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMTypeRef)handle_of(a2), a3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMBuilderGetDefaultFPMathTag(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMMetadataRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMBuilderGetDefaultFPMathTag((LLVMBuilderRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMBuilderSetDefaultFPMathTag(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMBuilderSetDefaultFPMathTag((LLVMBuilderRef)handle_of(a0), (LLVMMetadataRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

bool filc_native_LLVMCanValueUseFastMathFlags(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCanValueUseFastMathFlags((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMClearInsertionPosition(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMClearInsertionPosition((LLVMBuilderRef)handle_of(a0));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMCloneModule(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMModuleRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCloneModule((LLVMModuleRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_MODULE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstAdd((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstAddrSpaceCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstAddrSpaceCast((LLVMValueRef)handle_of(a0), (LLVMTypeRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstAllOnes(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstAllOnes((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstArray(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstArray((LLVMTypeRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstArray2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned long long p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstArray2((LLVMTypeRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstBitCast((LLVMValueRef)handle_of(a0), (LLVMTypeRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstExtractElement(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstExtractElement((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstGEP2((LLVMTypeRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2, p3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstGEPWithNoWrapFlags(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3, unsigned p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstGEPWithNoWrapFlags((LLVMTypeRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2, p3, p4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstInBoundsGEP2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, unsigned p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    LLVMValueRef* a2 = (LLVMValueRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstInBoundsGEP2((LLVMTypeRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2, p3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstInlineAsm(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, bool p3, bool p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstInlineAsm((LLVMTypeRef)handle_of(a0), a1, a2, p3, p4);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstInsertElement(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstInsertElement((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstInt(filc_thread* my_thread, filc_ptr p0, unsigned long long p1, bool p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstInt((LLVMTypeRef)handle_of(a0), p1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

long long filc_native_LLVMConstIntGetSExtValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    long long result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstIntGetSExtValue((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    long long result = (long long)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned long long filc_native_LLVMConstIntGetZExtValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned long long result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstIntGetZExtValue((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned long long result = (unsigned long long)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstIntOfString(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstIntOfString((LLVMTypeRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstIntOfStringAndSize(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, unsigned p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
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
    result_raw = api->LLVMConstIntOfStringAndSize((LLVMTypeRef)handle_of(a0), a1, p2, p3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstIntToPtr(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstIntToPtr((LLVMValueRef)handle_of(a0), (LLVMTypeRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstMul((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNSWAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNSWAdd((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNSWMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNSWMul((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNSWNeg(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNSWNeg((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNSWSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNSWSub((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNUWAdd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNUWAdd((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNUWMul(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNUWMul((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNUWSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNUWSub((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNamedStruct(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMValueRef* a1 = (LLVMValueRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNamedStruct((LLVMTypeRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNeg(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNeg((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNot(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNot((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstNull(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstNull((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstPointerCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstPointerCast((LLVMValueRef)handle_of(a0), (LLVMTypeRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstPointerNull(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstPointerNull((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstPtrToInt(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstPtrToInt((LLVMValueRef)handle_of(a0), (LLVMTypeRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstReal(filc_thread* my_thread, filc_ptr p0, double p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstReal((LLVMTypeRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstRealOfString(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstRealOfString((LLVMTypeRef)handle_of(a0), a1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstRealOfStringAndSize(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
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
    result_raw = api->LLVMConstRealOfStringAndSize((LLVMTypeRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstShuffleVector(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstShuffleVector((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMConstStringInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
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
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstStringInContext2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2, bool p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
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
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMConstStructInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMValueRef* a1 = (LLVMValueRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstStructInContext((LLVMContextRef)a0->state->handle, a1, p2, p3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstSub(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstSub((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstTrunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstTrunc((LLVMValueRef)handle_of(a0), (LLVMTypeRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstTruncOrBitCast(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstTruncOrBitCast((LLVMValueRef)handle_of(a0), (LLVMTypeRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstVector(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    LLVMValueRef* a0 = (LLVMValueRef*)decode_handle_array(my_thread, p0, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstVector(a0, p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstXor(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstXor((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMConstantPtrAuth(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2, filc_ptr p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a3 = unwrap_obj(my_thread, p3, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMConstantPtrAuth((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1), (LLVMValueRef)handle_of(a2), (LLVMValueRef)handle_of(a3));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMContextSetDiscardValueNames(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCountBasicBlocks((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMCountIncoming(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCountIncoming((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMCountParamTypes(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCountParamTypes((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMCountParams(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCountParams((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMCountStructElementTypes(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCountStructElementTypes((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateBasicBlockInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
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
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMCreateBuilderInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBuilderRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCreateBuilderInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BUILDER, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateEnumAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned long long p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAttributeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCreateEnumAttribute((LLVMContextRef)a0->state->handle, p1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateStringAttribute(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, filc_ptr p3, unsigned p4)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
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
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMCreateTypeAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAttributeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMCreateTypeAttribute((LLVMContextRef)a0->state->handle, p1, (LLVMTypeRef)handle_of(a2));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMDoubleTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMDoubleTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMDumpModule(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMDumpModule((LLVMModuleRef)handle_of(a0));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMDumpType(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMDumpType((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMDumpValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMDumpValue((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}


filc_ptr filc_native_LLVMFP128TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMFP128TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMFloatTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMFloatTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMFunctionType(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef* a1 = (LLVMTypeRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMFunctionType((LLVMTypeRef)handle_of(a0), a1, p2, p3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGEPGetNoWrapFlags(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMGEPNoWrapFlags result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGEPGetNoWrapFlags((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMGEPSetNoWrapFlags(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMGEPSetNoWrapFlags((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMGetAggregateElement(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetAggregateElement((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetAlignment(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetAlignment((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetAllocatedType(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetAllocatedType((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetArgOperand(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetArgOperand((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetArrayLength(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetArrayLength((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned long long filc_native_LLVMGetArrayLength2(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    uint64_t result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetArrayLength2((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned long long result = (unsigned long long)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetAtomicRMWBinOp(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAtomicRMWBinOp result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetAtomicRMWBinOp((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetAtomicSyncScopeID(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetAtomicSyncScopeID((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetAttributeCountAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetAttributeCountAtIndex((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBasicBlockParent(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetBasicBlockParent((LLVMBasicBlockRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBasicBlockTerminator(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetBasicBlockTerminator((LLVMBasicBlockRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBlockAddressBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetBlockAddressBasicBlock((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBlockAddressFunction(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetBlockAddressFunction((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetBuilderContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMContextRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetBuilderContext((LLVMBuilderRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_CONTEXT, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCallBrDefaultDest(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCallBrDefaultDest((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCallBrIndirectDest(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCallBrIndirectDest((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetCallBrNumIndirectDests(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCallBrNumIndirectDests((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetCallSiteAttributeCount(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCallSiteAttributeCount((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCallSiteEnumAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAttributeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCallSiteEnumAttribute((LLVMValueRef)handle_of(a0), p1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCallSiteStringAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, unsigned p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
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
    result_raw = api->LLVMGetCallSiteStringAttribute((LLVMValueRef)handle_of(a0), p1, a2, p3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCalledFunctionType(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCalledFunctionType((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCalledValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCalledValue((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetCastOpcode(filc_thread* my_thread, filc_ptr p0, bool p1, filc_ptr p2, bool p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMOpcode result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCastOpcode((LLVMValueRef)handle_of(a0), p1, (LLVMTypeRef)handle_of(a2), p3);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetClause(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetClause((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetCmpXchgFailureOrdering(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAtomicOrdering result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCmpXchgFailureOrdering((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetCmpXchgSuccessOrdering(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAtomicOrdering result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCmpXchgSuccessOrdering((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCondition(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCondition((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetConstOpcode(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMOpcode result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetConstOpcode((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetConstantPtrAuthAddrDiscriminator(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetConstantPtrAuthAddrDiscriminator((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetConstantPtrAuthDiscriminator(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetConstantPtrAuthDiscriminator((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetConstantPtrAuthKey(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetConstantPtrAuthKey((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetConstantPtrAuthPointer(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetConstantPtrAuthPointer((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCurrentDebugLocation(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCurrentDebugLocation((LLVMBuilderRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetCurrentDebugLocation2(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMMetadataRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetCurrentDebugLocation2((LLVMBuilderRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetDLLStorageClass(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMDLLStorageClass result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetDLLStorageClass((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetDebugLocColumn(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetDebugLocColumn((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetDebugLocLine(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetDebugLocLine((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetElementType(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetElementType((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetEntryBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetEntryBasicBlock((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetEnumAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAttributeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetEnumAttributeAtIndex((LLVMValueRef)handle_of(a0), p1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetEnumAttributeKind(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetEnumAttributeKind((LLVMAttributeRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetEnumAttributeKindForName(filc_thread* my_thread, filc_ptr p0, size_t p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    uint64_t result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetEnumAttributeValue((LLVMAttributeRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned long long result = (unsigned long long)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetExact(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetExact((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetFCmpPredicate(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMRealPredicate result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFCmpPredicate((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetFastMathFlags(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMFastMathFlags result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFastMathFlags((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstBasicBlock((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstDbgRecord(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMDbgRecordRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstDbgRecord((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_DBG_RECORD, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstFunction(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstFunction((LLVMModuleRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstGlobal(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstGlobal((LLVMModuleRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstGlobalAlias((LLVMModuleRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstGlobalIFunc((LLVMModuleRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstInstruction(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstInstruction((LLVMBasicBlockRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstNamedMetadata(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMNamedMDNodeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstNamedMetadata((LLVMModuleRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstParam(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstParam((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetFirstUse(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMUseRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFirstUse((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_USE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetFunctionCallConv(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetFunctionCallConv((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetGEPSourceElementType(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetGEPSourceElementType((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetGlobalIFuncResolver(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetGlobalIFuncResolver((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetGlobalParent(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMModuleRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetGlobalParent((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_MODULE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetICmpPredicate(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMIntPredicate result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetICmpPredicate((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetIncomingBlock(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetIncomingBlock((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetIncomingValue(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetIncomingValue((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInitializer(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInitializer((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInlineAsm(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2, filc_ptr p3, size_t p4, bool p5, bool p6, unsigned p7, bool p8)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
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
    result_raw = api->LLVMGetInlineAsm((LLVMTypeRef)handle_of(a0), a1, p2, a3, p4, p5, p6, p7, p8);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetInlineAsmCanUnwind(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInlineAsmCanUnwind((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetInlineAsmDialect(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMInlineAsmDialect result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInlineAsmDialect((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInlineAsmFunctionType(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInlineAsmFunctionType((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetInlineAsmHasSideEffects(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInlineAsmHasSideEffects((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetInlineAsmNeedsAlignedStack(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInlineAsmNeedsAlignedStack((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInsertBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInsertBlock((LLVMBuilderRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetInstructionCallConv(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInstructionCallConv((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetInstructionOpcode(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMOpcode result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInstructionOpcode((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetInstructionParent(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetInstructionParent((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetIntTypeWidth(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetIntTypeWidth((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetIntrinsicDeclaration(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, size_t p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    LLVMTypeRef* a2 = (LLVMTypeRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetIntrinsicDeclaration((LLVMModuleRef)handle_of(a0), p1, a2, p3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetIntrinsicID(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetIntrinsicID((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetIsDisjoint(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetIsDisjoint((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastBasicBlock((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastDbgRecord(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMDbgRecordRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastDbgRecord((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_DBG_RECORD, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetLastEnumAttributeKind(filc_thread* my_thread)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastFunction((LLVMModuleRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastGlobal(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastGlobal((LLVMModuleRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastGlobalAlias((LLVMModuleRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastGlobalIFunc((LLVMModuleRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastInstruction(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastInstruction((LLVMBasicBlockRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastNamedMetadata(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMNamedMDNodeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastNamedMetadata((LLVMModuleRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetLastParam(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLastParam((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetLinkage(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMLinkage result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetLinkage((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


unsigned filc_native_LLVMGetMDKindIDInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetMDNodeNumOperands((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

int filc_native_LLVMGetMaskValue(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    int result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetMaskValue((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    int result = (int)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetMetadata((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetModuleContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMContextRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetModuleContext((LLVMModuleRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_CONTEXT, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetModuleFlag(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
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
    result_raw = api->LLVMGetModuleFlag((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetNNeg(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNNeg((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetNSW(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNSW((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetNUW(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNUW((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedFunction(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNamedFunction((LLVMModuleRef)handle_of(a0), a1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedFunctionWithLength(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
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
    result_raw = api->LLVMGetNamedFunctionWithLength((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedGlobal(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNamedGlobal((LLVMModuleRef)handle_of(a0), a1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedGlobalAlias(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
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
    result_raw = api->LLVMGetNamedGlobalAlias((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedGlobalIFunc(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
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
    result_raw = api->LLVMGetNamedGlobalIFunc((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedGlobalWithLength(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
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
    result_raw = api->LLVMGetNamedGlobalWithLength((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNamedMetadata(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
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
    result_raw = api->LLVMGetNamedMetadata((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNamedMetadataNumOperands(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNamedMetadataNumOperands((LLVMModuleRef)handle_of(a0), a1);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextBasicBlock((LLVMBasicBlockRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextDbgRecord(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_DBG_RECORD, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMDbgRecordRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextDbgRecord((LLVMDbgRecordRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_DBG_RECORD, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextFunction(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextFunction((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextGlobal(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextGlobal((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextGlobalAlias((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextGlobalIFunc((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextInstruction(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextInstruction((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextNamedMetadata(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_NAMED_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMNamedMDNodeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextNamedMetadata((LLVMNamedMDNodeRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextParam(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextParam((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNextUse(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_USE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMUseRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNextUse((LLVMUseRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_USE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetNormalDest(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNormalDest((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNumArgOperands(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumArgOperands((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNumClauses(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumClauses((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNumContainedTypes(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumContainedTypes((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNumHandlers(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumHandlers((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNumIndices(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumIndices((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNumMaskElements(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumMaskElements((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

int filc_native_LLVMGetNumOperands(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    int result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumOperands((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    int result = (int)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetNumSuccessors(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetNumSuccessors((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetOperand(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetOperand((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetOperandUse(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMUseRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetOperandUse((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_USE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetOrInsertNamedMetadata(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
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
    result_raw = api->LLVMGetOrInsertNamedMetadata((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetOrdering(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMAtomicOrdering result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetOrdering((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetParam(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetParam((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetParamParent(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetParamParent((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetParentCatchSwitch(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetParentCatchSwitch((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPersonalityFn(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPersonalityFn((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetPointerAddressSpace(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPointerAddressSpace((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPoison(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPoison((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPrefixData(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPrefixData((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousBasicBlock((LLVMBasicBlockRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousDbgRecord(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_DBG_RECORD, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMDbgRecordRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousDbgRecord((LLVMDbgRecordRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_DBG_RECORD, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousFunction(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousFunction((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousGlobal(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousGlobal((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousGlobalAlias((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousGlobalIFunc((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousInstruction(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousInstruction((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousNamedMetadata(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_NAMED_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMNamedMDNodeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousNamedMetadata((LLVMNamedMDNodeRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_NAMED_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPreviousParam(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPreviousParam((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetPrologueData(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetPrologueData((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetReturnType(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetReturnType((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetStringAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, unsigned p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
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
    result_raw = api->LLVMGetStringAttributeAtIndex((LLVMValueRef)handle_of(a0), p1, a2, p3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_ATTRIBUTE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetSuccessor(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetSuccessor((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetSwitchDefaultDest(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetSwitchDefaultDest((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetSyncScopeID(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTailCallKind result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTailCallKind((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetTargetExtTypeIntParam(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTargetExtTypeIntParam((LLVMTypeRef)handle_of(a0), p1);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetTargetExtTypeNumIntParams(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTargetExtTypeNumIntParams((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetTargetExtTypeNumTypeParams(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTargetExtTypeNumTypeParams((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTargetExtTypeTypeParam(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTargetExtTypeTypeParam((LLVMTypeRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetThreadLocalMode(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMThreadLocalMode result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetThreadLocalMode((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTypeAttributeValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTypeAttributeValue((LLVMAttributeRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTypeByName(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTypeByName((LLVMModuleRef)handle_of(a0), a1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTypeByName2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
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
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetTypeContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMContextRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTypeContext((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_CONTEXT, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetTypeKind(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeKind result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetTypeKind((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetUndef(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetUndef((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

int filc_native_LLVMGetUndefMaskElem(filc_thread* my_thread)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMUnnamedAddr result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetUnnamedAddress((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetUnwindDest(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetUnwindDest((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetUsedValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_USE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetUsedValue((LLVMUseRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetUser(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_USE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetUser((LLVMUseRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGetValueContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMContextRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetValueContext((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_CONTEXT, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetValueKind(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueKind result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetValueKind((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetVectorSize(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetVectorSize((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMGetVisibility(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMVisibility result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetVisibility((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetVolatile(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetVolatile((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMGetWeak(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGetWeak((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMGlobalGetValueType(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMGlobalGetValueType((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMGlobalSetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMGlobalSetMetadata((LLVMValueRef)handle_of(a0), p1, (LLVMMetadataRef)handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}


filc_ptr filc_native_LLVMHalfTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMHalfTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

int filc_native_LLVMHasMetadata(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    int result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMHasMetadata((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    int result = (int)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMHasPersonalityFn(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMHasPersonalityFn((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMHasPrefixData(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMHasPrefixData((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMHasPrologueData(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMHasPrologueData((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMHasUnnamedAddr(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMHasUnnamedAddr((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMInsertBasicBlockInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInsertBasicBlockInContext((LLVMContextRef)a0->state->handle, (LLVMBasicBlockRef)handle_of(a1), a2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMInsertExistingBasicBlockAfterInsertBlock(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMInsertExistingBasicBlockAfterInsertBlock((LLVMBuilderRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMInsertIntoBuilder(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMInsertIntoBuilder((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (module)
        builder_adopt_module(my_thread, a0, module);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMInsertIntoBuilderWithName(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a2 = filc_check_and_get_tmp_str_or_null(my_thread, p2);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMInsertIntoBuilderWithName((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1), a2);
    filc_enter(my_thread);
    if (module)
        builder_adopt_module(my_thread, a0, module);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMInstructionClone(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInstructionClone((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMInt128TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInt128TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMInt16TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInt16TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMInt1TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInt1TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMInt32TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInt32TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMInt64TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInt64TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMInt8TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMInt8TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMIntTypeInContext(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIntTypeInContext((LLVMContextRef)a0->state->handle, p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIntrinsicGetType(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, size_t p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef* a2 = (LLVMTypeRef*)decode_handle_array(my_thread, p2, p3, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIntrinsicGetType((LLVMContextRef)a0->state->handle, p1, a2, p3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIntrinsicIsOverloaded(filc_thread* my_thread, unsigned p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAAddrSpaceCastInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAAllocaInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAAllocaInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAArgument(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAArgument((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAAtomicCmpXchgInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAAtomicCmpXchgInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAAtomicRMWInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAAtomicRMWInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsABasicBlock((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABinaryOperator(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsABinaryOperator((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABitCastInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsABitCastInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABlockAddress(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsABlockAddress((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsABranchInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsABranchInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACallBrInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACallBrInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACallInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACallInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACastInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACastInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACatchPadInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACatchPadInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACatchReturnInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACatchReturnInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACatchSwitchInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACatchSwitchInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACleanupPadInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACleanupPadInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACleanupReturnInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACleanupReturnInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsACmpInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsACmpInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstant(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstant((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantAggregateZero(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantAggregateZero((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantArray(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantArray((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantDataArray(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantDataArray((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantDataSequential(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantDataSequential((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantDataVector(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantDataVector((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantExpr(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantExpr((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantFP(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantFP((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantInt(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantInt((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantPointerNull(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantPointerNull((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantPtrAuth(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantPtrAuth((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantStruct(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantStruct((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantTokenNone(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantTokenNone((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAConstantVector(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAConstantVector((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsADbgDeclareInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsADbgDeclareInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsADbgInfoIntrinsic(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsADbgInfoIntrinsic((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsADbgLabelInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsADbgLabelInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsADbgVariableIntrinsic(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsADbgVariableIntrinsic((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAExtractElementInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAExtractElementInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAExtractValueInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAExtractValueInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFCmpInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFCmpInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFPExtInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFPExtInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFPToSIInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFPToSIInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFPToUIInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFPToUIInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFPTruncInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFPTruncInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFenceInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFenceInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFreezeInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFreezeInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFuncletPadInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFuncletPadInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAFunction(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAFunction((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGetElementPtrInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAGetElementPtrInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalAlias(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAGlobalAlias((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalIFunc(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAGlobalIFunc((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalObject(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAGlobalObject((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAGlobalValue((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAGlobalVariable(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAGlobalVariable((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAICmpInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAICmpInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAIndirectBrInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAIndirectBrInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInlineAsm(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAInlineAsm((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInsertElementInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAInsertElementInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInsertValueInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAInsertValueInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInstruction(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAInstruction((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAIntToPtrInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAIntToPtrInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAIntrinsicInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAIntrinsicInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAInvokeInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAInvokeInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsALandingPadInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsALandingPadInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsALoadInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsALoadInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAMemCpyInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAMemCpyInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAMemIntrinsic(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAMemIntrinsic((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAMemMoveInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAMemMoveInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAMemSetInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAMemSetInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAPHINode(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAPHINode((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAPoisonValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAPoisonValue((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAPtrToIntInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAPtrToIntInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAResumeInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAResumeInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAReturnInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAReturnInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsASExtInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsASExtInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsASIToFPInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsASIToFPInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsASelectInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsASelectInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAShuffleVectorInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAShuffleVectorInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAStoreInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAStoreInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsASwitchInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsASwitchInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsATruncInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsATruncInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUIToFPInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAUIToFPInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUnaryInstruction(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAUnaryInstruction((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUnaryOperator(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAUnaryOperator((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUndefValue(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAUndefValue((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUnreachableInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAUnreachableInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAUser(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAUser((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAVAArgInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAVAArgInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMIsAZExtInst(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsAZExtInst((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsCleanup(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsCleanup((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsConditional(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsConditional((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsConstant(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsConstant((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsConstantString(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsConstantString((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsDeclaration(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsDeclaration((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsEnumAttribute(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsEnumAttribute((LLVMAttributeRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsExternallyInitialized(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsExternallyInitialized((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsFunctionVarArg(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsFunctionVarArg((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsGlobalConstant(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsGlobalConstant((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsInBounds(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsInBounds((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsLiteralStruct(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsLiteralStruct((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsMultithreaded(filc_thread* my_thread)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsNewDbgInfoFormat((LLVMModuleRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsNull(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsNull((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsOpaqueStruct(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsOpaqueStruct((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsPackedStruct(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsPackedStruct((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsPoison(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsPoison((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsStringAttribute(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsStringAttribute((LLVMAttributeRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsTailCall(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsTailCall((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsThreadLocal(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsThreadLocal((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsTypeAttribute(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_ATTRIBUTE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsTypeAttribute((LLVMAttributeRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMIsUndef(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMIsUndef((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMLabelTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMLabelTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMLookupIntrinsicID(filc_thread* my_thread, filc_ptr p0, size_t p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
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
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMValueRef* a1 = (LLVMValueRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMMDNodeInContext((LLVMContextRef)a0->state->handle, a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMMDNodeInContext2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMMetadataRef* a1 = (LLVMMetadataRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMMetadataRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMMDNodeInContext2((LLVMContextRef)a0->state->handle, a1, p2);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMMDStringInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
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
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMMDStringInContext2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
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
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMMetadataAsValue(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMMetadataAsValue((LLVMContextRef)a0->state->handle, (LLVMMetadataRef)handle_of(a1));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMMetadataTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMMetadataTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMModuleCreateWithNameInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    const char* a0 = filc_check_and_get_tmp_str_or_null(my_thread, p0);
    filc_llvm_jit_context_object* a1 = unwrap_context(my_thread, p1);
    context = a1;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMModuleRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMModuleCreateWithNameInContext(a0, (LLVMContextRef)a1->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_MODULE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMModuleFlagEntriesGetFlagBehavior(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMModuleFlagBehavior result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMModuleFlagEntriesGetFlagBehavior((LLVMModuleFlagEntry*)handle_of(a0), p1);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMModuleFlagEntriesGetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE_FLAG_ENTRIES, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMMetadataRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMModuleFlagEntriesGetMetadata((LLVMModuleFlagEntry*)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMMoveBasicBlockAfter(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMMoveBasicBlockAfter((LLVMBasicBlockRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMMoveBasicBlockBefore(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMMoveBasicBlockBefore((LLVMBasicBlockRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}


filc_ptr filc_native_LLVMPPCFP128TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMPPCFP128TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMPointerType(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMPointerType((LLVMTypeRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMPointerTypeInContext(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMPointerTypeInContext((LLVMContextRef)a0->state->handle, p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMPointerTypeIsOpaque(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMPointerTypeIsOpaque((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMPositionBuilder(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMPositionBuilder((LLVMBuilderRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1), (LLVMValueRef)handle_of(a2));
    filc_enter(my_thread);
    if (module)
        builder_adopt_module(my_thread, a0, module);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMPositionBuilderAtEnd(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMPositionBuilderAtEnd((LLVMBuilderRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    filc_enter(my_thread);
    if (module)
        builder_adopt_module(my_thread, a0, module);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMPositionBuilderBefore(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMPositionBuilderBefore((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (module)
        builder_adopt_module(my_thread, a0, module);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMPositionBuilderBeforeDbgRecords(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMPositionBuilderBeforeDbgRecords((LLVMBuilderRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1), (LLVMValueRef)handle_of(a2));
    filc_enter(my_thread);
    if (module)
        builder_adopt_module(my_thread, a0, module);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMPositionBuilderBeforeInstrAndDbgRecords(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMPositionBuilderBeforeInstrAndDbgRecords((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (module)
        builder_adopt_module(my_thread, a0, module);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMRemoveCallSiteEnumAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMRemoveCallSiteEnumAttribute((LLVMValueRef)handle_of(a0), p1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMRemoveCallSiteStringAttribute(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, unsigned p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
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
    api->LLVMRemoveCallSiteStringAttribute((LLVMValueRef)handle_of(a0), p1, a2, p3);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMRemoveEnumAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMRemoveEnumAttributeAtIndex((LLVMValueRef)handle_of(a0), p1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMRemoveStringAttributeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2, unsigned p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
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
    api->LLVMRemoveStringAttributeAtIndex((LLVMValueRef)handle_of(a0), p1, a2, p3);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMReplaceAllUsesWith(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMReplaceAllUsesWith((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMReplaceMDNodeOperandWith(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMReplaceMDNodeOperandWith((LLVMValueRef)handle_of(a0), p1, (LLVMMetadataRef)handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMScalableVectorType(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMScalableVectorType((LLVMTypeRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMSetAlignment(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetAlignment((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetArgOperand(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetArgOperand((LLVMValueRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetAtomicRMWBinOp(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetAtomicRMWBinOp((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetAtomicSingleThread(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetAtomicSingleThread((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetAtomicSyncScopeID(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetAtomicSyncScopeID((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetCleanup(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetCleanup((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetCmpXchgFailureOrdering(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetCmpXchgFailureOrdering((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetCmpXchgSuccessOrdering(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetCmpXchgSuccessOrdering((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetCondition(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetCondition((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetCurrentDebugLocation(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetCurrentDebugLocation((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetCurrentDebugLocation2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_METADATA, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetCurrentDebugLocation2((LLVMBuilderRef)handle_of(a0), (LLVMMetadataRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetDLLStorageClass(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetDLLStorageClass((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetDataLayout(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetDataLayout((LLVMModuleRef)handle_of(a0), a1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetExact(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetExact((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetExternallyInitialized(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetExternallyInitialized((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetFastMathFlags(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetFastMathFlags((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetFunctionCallConv(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetFunctionCallConv((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetGC(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetGC((LLVMValueRef)handle_of(a0), a1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetGlobalConstant(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetGlobalConstant((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetGlobalIFuncResolver(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetGlobalIFuncResolver((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetInitializer(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetInitializer((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetInstDebugLocation(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_BUILDER, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetInstDebugLocation((LLVMBuilderRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetInstrParamAlignment(filc_thread* my_thread, filc_ptr p0, unsigned p1, unsigned p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetInstrParamAlignment((LLVMValueRef)handle_of(a0), p1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetInstructionCallConv(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetInstructionCallConv((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetIsDisjoint(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetIsDisjoint((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetIsInBounds(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetIsInBounds((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetIsNewDbgInfoFormat(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetIsNewDbgInfoFormat((LLVMModuleRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetLinkage(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetLinkage((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetMetadata((LLVMValueRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetModuleIdentifier(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
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
    api->LLVMSetModuleIdentifier((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetModuleInlineAsm(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetModuleInlineAsm((LLVMModuleRef)handle_of(a0), a1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetModuleInlineAsm2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
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
    api->LLVMSetModuleInlineAsm2((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetNNeg(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetNNeg((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetNSW(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetNSW((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetNUW(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetNUW((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetNormalDest(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetNormalDest((LLVMValueRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetOperand(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetOperand((LLVMValueRef)handle_of(a0), p1, (LLVMValueRef)handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetOrdering(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetOrdering((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetParamAlignment(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetParamAlignment((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetParentCatchSwitch(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetParentCatchSwitch((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetPersonalityFn(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetPersonalityFn((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetPrefixData(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetPrefixData((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetPrologueData(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetPrologueData((LLVMValueRef)handle_of(a0), (LLVMValueRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetSection(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetSection((LLVMValueRef)handle_of(a0), a1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetSourceFileName(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
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
    api->LLVMSetSourceFileName((LLVMModuleRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetSuccessor(filc_thread* my_thread, filc_ptr p0, unsigned p1, filc_ptr p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a2 = unwrap_obj(my_thread, p2, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetSuccessor((LLVMValueRef)handle_of(a0), p1, (LLVMBasicBlockRef)handle_of(a2));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetTailCall(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetTailCall((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetTailCallKind(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetTailCallKind((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetTarget(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_MODULE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetTarget((LLVMModuleRef)handle_of(a0), a1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetThreadLocal(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetThreadLocal((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetThreadLocalMode(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetThreadLocalMode((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetUnnamedAddr(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetUnnamedAddr((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetUnnamedAddress(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetUnnamedAddress((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetUnwindDest(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_object* a1 = unwrap_obj(my_thread, p1, FILC_LLVM_JIT_KIND_BASIC_BLOCK, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetUnwindDest((LLVMValueRef)handle_of(a0), (LLVMBasicBlockRef)handle_of(a1));
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetValueName(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    const char* a1 = filc_check_and_get_tmp_str_or_null(my_thread, p1);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetValueName((LLVMValueRef)handle_of(a0), a1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetValueName2(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, size_t p2)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
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
    api->LLVMSetValueName2((LLVMValueRef)handle_of(a0), a1, p2);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetVisibility(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetVisibility((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetVolatile(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetVolatile((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

void filc_native_LLVMSetWeak(filc_thread* my_thread, filc_ptr p0, bool p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMSetWeak((LLVMValueRef)handle_of(a0), p1);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}

filc_ptr filc_native_LLVMSizeOf(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMValueRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMSizeOf((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_VALUE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMStructCreateNamed(filc_thread* my_thread, filc_ptr p0, filc_ptr p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
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
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMStructGetTypeAtIndex(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMStructGetTypeAtIndex((LLVMTypeRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

void filc_native_LLVMStructSetBody(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    LLVMTypeRef* a1 = (LLVMTypeRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    api->LLVMStructSetBody((LLVMTypeRef)handle_of(a0), a1, p2, p3);
    filc_enter(my_thread);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
}


filc_ptr filc_native_LLVMStructTypeInContext(filc_thread* my_thread, filc_ptr p0, filc_ptr p1, unsigned p2, bool p3)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    LLVMTypeRef* a1 = (LLVMTypeRef*)decode_handle_array(my_thread, p1, p2, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMStructTypeInContext((LLVMContextRef)a0->state->handle, a1, p2, p3);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMTokenTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMTokenTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMTypeIsSized(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMTypeIsSized((LLVMTypeRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMTypeOf(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMTypeOf((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMValueAsBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBasicBlockRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMValueAsBasicBlock((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_BASIC_BLOCK, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMValueAsMetadata(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMMetadataRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMValueAsMetadata((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

bool filc_native_LLVMValueIsBasicBlock(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_VALUE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMBool result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMValueIsBasicBlock((LLVMValueRef)handle_of(a0));
    filc_enter(my_thread);
    bool result = (bool)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

unsigned filc_native_LLVMValueMetadataEntriesGetKind(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_METADATA_ENTRIES, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    unsigned result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMValueMetadataEntriesGetKind((LLVMValueMetadataEntry*)handle_of(a0), p1);
    filc_enter(my_thread);
    unsigned result = (unsigned)result_raw;
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMValueMetadataEntriesGetMetadata(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_METADATA_ENTRIES, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMMetadataRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMValueMetadataEntriesGetMetadata((LLVMValueMetadataEntry*)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_METADATA, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

filc_ptr filc_native_LLVMVectorType(filc_thread* my_thread, filc_ptr p0, unsigned p1)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_object* a0 = unwrap_obj(my_thread, p0, FILC_LLVM_JIT_KIND_TYPE, &context, &module);
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMVectorType((LLVMTypeRef)handle_of(a0), p1);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMVoidTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMVoidTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMX86AMXTypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMX86AMXTypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}


filc_ptr filc_native_LLVMX86FP80TypeInContext(filc_thread* my_thread, filc_ptr p0)
{
    const struct filc_llvm_jit_api* api = ensure_api(my_thread);
    filc_llvm_jit_context_object* context = NULL;
    filc_llvm_jit_object* module = NULL;
    filc_llvm_jit_context_object* a0 = unwrap_context(my_thread, p0);
    context = a0;
    filc_llvm_jit_context_state* lock_state =
        context ? context->state : NULL;
    LLVMTypeRef result_raw;
    if (lock_state)
        pas_lock_lock(&lock_state->lock);
    filc_exit(my_thread);
    result_raw = api->LLVMX86FP80TypeInContext((LLVMContextRef)a0->state->handle);
    filc_enter(my_thread);
    filc_ptr result = wrap_handle(my_thread, context, module, FILC_LLVM_JIT_KIND_TYPE, (void*)result_raw);
    if (lock_state)
        pas_lock_unlock(&lock_state->lock);
    return result;
}

#endif /* PAS_ENABLE_FILC */

#endif /* LIBPAS_ENABLED */
