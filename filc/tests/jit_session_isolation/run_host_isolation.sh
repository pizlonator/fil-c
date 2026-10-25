#!/bin/sh
# Host proof: two shim sessions with different resolve
# callbacks for the SAME extern name must each resolve through their own
# callback.  The harness drives libfilcllvmjit.so purely through its public
# FilLLVMJITApi table (no LLVM headers/libs needed): it builds two one-call
# modules via the table's LLVM C API function pointers, sets per-session
# callbacks returning different addresses, and calls each session's
# pizlonated getter (which tail-calls the lazy worker, routed through the
# session-carrying wrapper AddModule authors).  Pre-fix, the shim's
# scan-for-max-Seq shared resolver handed BOTH sessions the newest
# callback, so session 1's flight would carry session 2's address.
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
#include <dlfcn.h>

#include "filc_llvm_jit_api.h"

static const struct FilLLVMJITApi *api;
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
    const struct FilLLVMJITApi *(*getapi)(void) =
        (const struct FilLLVMJITApi *(*)(void))dlsym(h, "FilLLVMJIT_GetApi");
    if (!getapi) {
        printf("no FilLLVMJIT_GetApi\n");
        return 1;
    }
    api = getapi();
    printf("shim API version == %llu\n", api->version);
    if (api->version != 4) {
        printf("want API version 4\n");
        return 1;
    }
    /* No explicit init: first Create lazily boots the shim (API v4). */
    FilLLVMJITRef s1 = api->Create(NULL);
    FilLLVMJITRef s2 = api->Create(NULL);
    if (!s1 || !s2) {
        printf("create failed\n");
        return 1;
    }
    api->SetResolveCallback(s1, cb1, NULL);
    api->SetResolveCallback(s2, cb2, NULL);
    LLVMModuleRef m1 = build_module("run_iso1");
    LLVMModuleRef m2 = build_module("run_iso2");
    FilLLVMJITModuleRef o1 = 0, o2 = 0;
    if (api->AddModule(s1, m1, "m1", &o1)) {
        printf("add1 failed: %s\n", api->GetLastError());
        return 1;
    }
    if (api->AddModule(s2, m2, "m2", &o2)) {
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
       through this session's callback via the bound wrapper. */
    WorkerFn w1 = (WorkerFn)api->GetFunctionAddress(s1, "shared_ext");
    WorkerFn w2 = (WorkerFn)api->GetFunctionAddress(s2, "shared_ext");
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
    /* Informational: the legacy shared helper keeps last-wins scan semantics
       for ABI stability; only module-bound resolution is session-routed. */
    printf("legacy ResolveByName == %p (last-wins, informational)\n",
           api->ResolveByName("shared_ext"));
    printf("session isolation harness OK\n");
    return 0;
}
HARNESS_EOF
cc -O1 -g -o "$HARNESS_BIN" "$HARNESS_C" -I "$ROOT/filc/llvmjit" -ldl
LD_LIBRARY_PATH="$ROOT/pizfix/lib" "$HARNESS_BIN" \
    "$ROOT/pizfix/libexec/libfilcllvmjit.so" "$ROOT/pizfix/lib"
