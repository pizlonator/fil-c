#!/bin/sh
# Host proof: two shim sessions with different resolve
# callbacks for the SAME extern name must each resolve through their own
# callback.  The harness drives libfilcllvmjit.so purely through its public
# filc_llvm_jit_api table (no LLVM headers/libs needed): it builds two one-call
# modules via the table's LLVM C API function pointers, installs per-session
# memory callbacks whose resolve entry points return different addresses,
# and calls each module's pizlonated getter (which tail-calls the lazy
# worker, routed through the module-private wrapper CompileModule authors
# around that compile's callbacks).  Each module passes its own
# filc_jit_module token, and the resolve entry points check it, so a
# cross-module mixup fails loudly.  Sessions are then destroyed while the
# modules stay callable, proving resolution state lives in the module.
#
# Usage: sh filc/tests/jit_session_isolation/run_host_isolation.sh
# (from the repository root).  Also invoked by jit_session_isolation.c
# under filc/run-tests.
set -e
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HARNESS_C="$(mktemp /tmp/iso_harness.XXXXXX.c)"
HARNESS_BIN="$(mktemp /tmp/iso_harness.XXXXXX)"
trap 'rm -f "$HARNESS_C" "$HARNESS_BIN"' EXIT INT TERM
cat > "$HARNESS_C" <<'HARNESS_EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>
#include <sys/mman.h>

#include "filc_llvm_jit_api.h"

static const struct filc_llvm_jit_api *api;
static int val1 = 111, val2 = 222;

static void *cb1(const char *name, void *ctx)
{
    (void)ctx;
    if (!strcmp(name, "shared_ext"))
        return &val1;
    return NULL;
}

static void *cb2(const char *name, void *ctx)
{
    (void)ctx;
    if (!strcmp(name, "shared_ext"))
        return &val2;
    return NULL;
}

struct SessionState {
    void *(*resolver)(const char *name, void *ctx);
    void *resolverCtx;
    void *expectedModule;
    int resolveCalls;
};

static void *harness_allocate_jit_module(void *context,
                                         unsigned long long aux,
                                         unsigned long long other)
{
    (void)aux;
    (void)other;
    return ((struct SessionState *)context)->expectedModule;
}

static void *harness_allocate_executable(void *context, void *jit_module,
                                         unsigned long long size,
                                         unsigned long long alignment,
                                         const char *section_name)
{
    (void)context;
    (void)jit_module;
    (void)section_name;
    size_t align = (size_t)alignment < 16 ? 16 : (size_t)alignment;
    size_t total = (size_t)size + align;
    void *mem = mmap(NULL, total, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED)
        return NULL;
    uintptr_t addr = ((uintptr_t)mem + align - 1) & ~(uintptr_t)(align - 1);
    return (void *)addr;
}

static void *harness_allocate_object(void *context, void *jit_module,
                                     const char *section_name,
                                     unsigned long long size,
                                     unsigned long long alignment)
{
    (void)context;
    (void)jit_module;
    (void)section_name;
    (void)alignment;
    return malloc(size ? (size_t)size : 1);
}

static void *harness_allocate_module_data(void *context, void *jit_module,
                                          const char *section_name,
                                          unsigned long long size,
                                          unsigned long long alignment)
{
    (void)context;
    (void)jit_module;
    (void)section_name;
    (void)alignment;
    return malloc(size ? (size_t)size : 1);
}

static void harness_finalize_object(void *context, void *jit_module,
                                    void *object, unsigned long long size)
{
    (void)context;
    (void)jit_module;
    (void)object;
    (void)size;
}

static void *harness_resolve_symbol(void *context, void *jit_module,
                                    const char *name)
{
    struct SessionState *st = (struct SessionState *)context;
    if (jit_module != st->expectedModule) {
        printf("resolve saw wrong module token\n");
        exit(1);
    }
    st->resolveCalls++;
    return st->resolver(name, st->resolverCtx);
}

static void *harness_resolve_raw_pointer(void *context, void *jit_module,
                                         unsigned long long encoded)
{
    (void)context;
    (void)jit_module;
    return (void *)(uintptr_t)encoded;
}

typedef struct {
    void *ptr;
    void *lower;
} Flight;
typedef Flight (*WorkerFn)(void *, void *);

static LLVMModuleRef build_module(const char *runname)
{
    LLVMContextRef ctx = api->LLVMContextCreate();
    LLVMTypeRef i32 = api->LLVMInt32TypeInContext(ctx);
    LLVMModuleRef mod = api->LLVMModuleCreateWithNameInContext("m", ctx);
    LLVMTypeRef extTy = api->LLVMFunctionType(i32, NULL, 0, 0);
    LLVMValueRef ext = api->LLVMAddFunction(mod, "shared_ext", extTy);
    LLVMTypeRef runTy = api->LLVMFunctionType(i32, NULL, 0, 0);
    LLVMValueRef run = api->LLVMAddFunction(mod, runname, runTy);
    LLVMBasicBlockRef bb = api->LLVMAppendBasicBlockInContext(ctx, run, "e");
    LLVMBuilderRef b = api->LLVMCreateBuilderInContext(ctx);
    api->LLVMPositionBuilderAtEnd(b, bb);
    LLVMValueRef call = api->LLVMBuildCall2(b, extTy, ext, NULL, 0, "c");
    api->LLVMBuildRet(b, call);
    return mod;
}

int main(int argc, char **argv)
{
    const char *so = argv[1];
    const char *libdir = argv[2];
    char pizlo[4096];
    /* The shim links -nodefaultlibs (for dlopen from Fil-C processes) and
       leaves libm symbols undefined; make the host's libm global first. */
    dlopen("libm.so.6", RTLD_NOW | RTLD_GLOBAL);
    /* JITed modules reference Fil-C runtime helpers (pollchecks, failure
       paths).  In production those resolve from the hosting Fil-C process;
       standalone, expose libpizlo's symbols the same way.  The harness only
       runs the resolve-cache path, which never calls them. */
    snprintf(pizlo, sizeof(pizlo), "%s/libpizlo.so", libdir);
    if (!dlopen(pizlo, RTLD_NOW | RTLD_GLOBAL))
        printf("note: no libpizlo: %s\n", dlerror());
    void *h = dlopen(so, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        printf("dlopen failed: %s\n", dlerror());
        return 1;
    }
    const struct filc_llvm_jit_api *(*getapi)(void) =
        (const struct filc_llvm_jit_api *(*)(void))dlsym(h, "FilLLVMJIT_GetApi");
    if (!getapi) {
        printf("no FilLLVMJIT_GetApi\n");
        return 1;
    }
    api = getapi();
    printf("shim API version == %llu\n", api->version);
    if (api->version != 7) {
        printf("want API version 7\n");
        return 1;
    }
    /* No explicit init: first Create lazily boots the shim. */
    ShimJit s1 = api->Create();
    ShimJit s2 = api->Create();
    if (!s1 || !s2) {
        printf("create failed\n");
        return 1;
    }
    /* Each session installs its own memory callbacks; the resolve entry
       points return different addresses and check the module token. */
    static char token1, token2;
    struct SessionState st1 = { cb1, NULL, &token1, 0 };
    struct SessionState st2 = { cb2, NULL, &token2, 0 };
    struct filc_llvm_jit_memory_callbacks cbs1 = {
        &st1,
        harness_allocate_jit_module,
        harness_allocate_executable,
        harness_allocate_object,
        harness_allocate_module_data,
        harness_finalize_object,
        harness_resolve_symbol,
        harness_resolve_raw_pointer,
    };
    struct filc_llvm_jit_memory_callbacks cbs2 = cbs1;
    cbs2.context = &st2;
    if (api->SetMemoryCallbacks(s1, &cbs1)) {
        printf("callbacks1 failed: %s\n", api->GetLastError());
        return 1;
    }
    if (api->SetMemoryCallbacks(s2, &cbs2)) {
        printf("callbacks2 failed: %s\n", api->GetLastError());
        return 1;
    }
    LLVMModuleRef m1 = build_module("run_iso1");
    LLVMModuleRef m2 = build_module("run_iso2");
    ShimJitModule o1 = 0, o2 = 0;
    o1 = api->CompileModule(s1, m1, "m1", &token1);
    if (!o1) {
        printf("add1 failed: %s\n", api->GetLastError());
        return 1;
    }
    o2 = api->CompileModule(s2, m2, "m2", &token2);
    if (!o2) {
        printf("add2 failed: %s\n", api->GetLastError());
        return 1;
    }
    if (!o1 || !o2) {
        printf("null module handle\n");
        return 1;
    }
    /* The lazy worker (__filc_jit_get_*) is internal linkage; resolve the
       public pizlonated getter instead (exact name misses, the documented
       pizlonated_ retry hits).  It tail-calls the worker, which routes
       through this module's callbacks via the bound wrapper. */
    WorkerFn w1 = (WorkerFn)api->GetFunctionAddress(o1, "shared_ext");
    WorkerFn w2 = (WorkerFn)api->GetFunctionAddress(o2, "shared_ext");
    if (!w1 || !w2) {
        printf("getter lookup failed: %s\n", api->GetLastError());
        return 1;
    }
    Flight f1 = w1(NULL, NULL);
    Flight f2 = w2(NULL, NULL);
    printf("session1 flight == {%p, %p} (want {%p, %p})\n", f1.ptr, f1.lower,
           (void *)&val1, (void *)&val1);
    printf("session2 flight == {%p, %p} (want {%p, %p})\n", f2.ptr, f2.lower,
           (void *)&val2, (void *)&val2);
    if (f1.ptr != (void *)&val1 || f1.lower != (void *)&val1) {
        printf("session1 misrouted\n");
        return 1;
    }
    if (f2.ptr != (void *)&val2 || f2.lower != (void *)&val2) {
        printf("session2 misrouted\n");
        return 1;
    }
    if (!st1.resolveCalls || !st2.resolveCalls) {
        printf("resolve callbacks never ran\n");
        return 1;
    }
    /* Sessions are mortal: destroy both, then keep calling through the
       modules.  Lookups and calls must keep working on module state alone. */
    api->DestroySession(s1);
    api->DestroySession(s2);
    WorkerFn w1b = (WorkerFn)api->GetFunctionAddress(o1, "shared_ext");
    WorkerFn w2b = (WorkerFn)api->GetFunctionAddress(o2, "shared_ext");
    if (!w1b || !w2b) {
        printf("post-session lookup failed: %s\n", api->GetLastError());
        return 1;
    }
    Flight f1b = w1b(NULL, NULL);
    Flight f2b = w2b(NULL, NULL);
    if (f1b.ptr != (void *)&val1 || f2b.ptr != (void *)&val2) {
        printf("post-session call misrouted\n");
        return 1;
    }
    api->DestroyModule(o1);
    api->DestroyModule(o2);
    printf("session isolation harness OK\n");
    return 0;
}
HARNESS_EOF
cc -O1 -g -o "$HARNESS_BIN" "$HARNESS_C" -I "$ROOT/filc/llvmjit" -ldl
LD_LIBRARY_PATH="$ROOT/pizfix/lib" "$HARNESS_BIN" \
    "$ROOT/pizfix/libexec/libfilcllvmjit.so" "$ROOT/pizfix/lib"
