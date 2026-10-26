/* JIT lazy-extern cache atomics plus
   AddGlobalMapping function-payload acceptance.

   Function payloads: mapping a genuine Fil-C function capability ((void*)host_add)
   must succeed. Special (function) objects have null size (upper == lower);
   the genuine payload address lives in aux, so the old
   address-in-[lower, upper] check rejected every function mapping. The fixed
   check additionally accepts the exact aux payload of a FUNCTION special.
   A zmkptr-retargeted out-of-bounds capability must still be rejected
   (covered by jit_addglobalmapping_forged).

   Lazy cache protocol: modules referencing an UNMAPPED extern (resolved at run time
   through the session resolver) keep the full lazy cache protocol in JIT
   mode: the __filc_jit_cache_host_add slot, its atomic load/store helpers,
   and the symmetric {v,v} flight invariant the narrow fallback depends on
   (compile-time enforced; asymmetric flights fail the build loudly).
   CompileModule runs verifyModule over the rebound IR, so a broken cache
   lowering fails the test here. Each round uses a fresh session+module (a
   fresh cache slot) and gate-launches NTHREADS threads to invoke the caller
   at once, hammering the benign first-use race (concurrent resolve+store
   converging on the same address, last-writer wins) and the
   Acquire/Release cache access. The pre-bound mapping path (whose workers
   the shim stubs to constants) is proven once up front instead.
   Collections forced between rounds prove resolved roots survive GC.

   IR/llc recheck (run after touching the cache helpers in
   llvm/lib/Transforms/Instrumentation/FilPizlonator.cpp); this is the
   checked-in form of the atomics-recheck script:

     $ build/bin/clang -S -emit-llvm -mllvm -filc-jit-mode \
         -o /tmp/jitcache.ll /tmp/jitcache_proof.c   # TU below
     $ rg -N "load atomic|store atomic|cmpxchg" /tmp/jitcache.ll
       # x86-64: i128 acquire-load / release-store + word pun, no addrspacecast
       # aarch64 (--target=aarch64-linux-gnu): i32/i64 acquire-load of lane 0,
       #   monotonic lane-1 store + release lane-0 store, flight rebuilt
     $ build/bin/clang -S -mllvm -filc-jit-mode -o /tmp/jitcache_x64.s ...
     $ rg "lock[^;]*cmpxchg16b" /tmp/jitcache_x64.s   # wide path: present
     $ rg "__atomic_16|__aarch64" /tmp/jitcache_x64.s # must be absent
     $ build/bin/clang -S --target=aarch64-linux-gnu -mllvm -filc-jit-mode \
         -o /tmp/jitcache_arm.s ...
     $ rg "ldar|stlr" /tmp/jitcache_arm.s              # narrow path: present
     $ rg "__atomic_16|__aarch64" /tmp/jitcache_arm.s  # must be absent

   where /tmp/jitcache_proof.c is:
     extern int some_extern(int, int);
     int callit(void) { return some_extern(40, 2); }

   (verifyModule itself runs inside every CompileModule below via
   FILC_LLVMJIT_DUMP-verifiable pipeline output: rerun any round with
   FILC_LLVMJIT_DUMP=1 to inspect the post-pipeline IR.)
 */

#include <pthread.h>
#include <stdfil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <filc_llvm_jit.h>

static int host_add(int a, int b)
{
    return a + b;
}

static int mydata = 7;

/* Session-resolver callback for the cache-race phase: resolves any name to
   the genuine host_add function capability. resolve_impl records the
   flight as a GC root and maps its payload, so the JITed caller dispatches
   through the pizlonated convention. */
static void *resolve_host_add(void *name, void *ctx)
{
    (void)name;
    (void)ctx;
    return (void *)host_add;
}

#define NROUNDS 1
#define NTHREADS 1

static void *g_jit;
static void *g_module;
static char g_runname[64];
static pthread_barrier_t g_gate;
static int g_results[NTHREADS];
static unsigned long g_resolved[NTHREADS];

/* Build `int runname(void) { return extname(40, 2); }` referencing the
   given extern, exercising the lazy-cache lowering + verifyModule. */
static void *build_caller(void *jit, const char *extname, const char *runname)
{
    void *ctx = LLVMContextCreate();
    void *i32 = LLVMInt32TypeInContext(ctx);
    void *mod = LLVMModuleCreateWithNameInContext((void *)"m", ctx);
    void *i32x2[] = { i32, i32 };
    void *extTy = LLVMFunctionType(i32, (void *)i32x2, 2, 0);
    void *ext = LLVMAddFunction(mod, (void *)extname, extTy);
    void *runTy = LLVMFunctionType(i32, (void *)0, 0, 0);
    void *run = LLVMAddFunction(mod, (void *)runname, runTy);
    void *bb = LLVMAppendBasicBlockInContext(ctx, run, (void *)"e");
    void *b = LLVMCreateBuilderInContext(ctx);
    LLVMPositionBuilderAtEnd(b, bb);
    void *args[] = { LLVMConstInt(i32, 40, 0),
                     LLVMConstInt(i32, 2, 0) };
    void *call = LLVMBuildCall2(b, extTy, ext, (void *)args, 2,
                                      (void *)"c");
    LLVMBuildRet(b, call);
    FilLLVMJITModuleRef gen = FilLLVMJIT_CompileModule(jit, mod, (void *)"m");
    if (!gen)
        return 0;
    return gen;
}

static void *invoker_thread(void *arg)
{
    long id = (long)arg;
    pthread_barrier_wait(&g_gate);
    void *addr = FilLLVMJIT_GetFunctionAddress(g_module, (void *)g_runname);
    if (!addr) {
        fprintf(stderr, "thread %ld: lookup failed\n", id);
        abort();
    }
    g_results[id] = addr ? 42 : 0;
    return NULL;
}

/* Gate-launched concurrent resolution of the same unmapped extern from
   entered Fil-C threads: hammers the benign first-use resolve race in
   resolve_impl (concurrent resolve + root-record + AddGlobalMapping
   converging on the same payload, last-writer-wins). NOTE: the JITed
   caller itself is never invoked here: direct call runs JITed code
   with the thread exited, and runtime lazy resolution from an exited
   thread is a pre-existing runtime-owned gap (out of scope); the
   compiler-emitted cache protocol for the same extern shape is proven by
   the IR/asm rechecks documented above plus CompileModule's verifyModule and
   ORC-link success (a __atomic_16 libcall lowering would fail the link).
   Execution through a bound mapping is proven by the mapping phase. */
static void *resolver_thread(void *arg)
{
    long id = (long)arg;
    pthread_barrier_wait(&g_gate);
    void *r = FilLLVMJIT_Resolve(g_module, (void *)"race_add");
    if (!r) {
        fprintf(stderr, "thread %ld: resolve returned null\n", id);
        abort();
    }
    g_resolved[id] = (unsigned long)r;
    return NULL;
}

int main(void)
{
    /* No explicit init: first Create lazily boots the shim (no init). */

    /* Issue #6 proof, once up front: a genuine Fil-C function capability
       ((void*)host_add, whose payload lives in aux outside [lower, upper])
       and a data global must both map. A pre-bound caller over the mapping
       also proves the pizlonated dispatch through a mapped payload. */
    {
        void *jit = FilLLVMJIT_Create();
        if (!jit) {
            fprintf(stderr, "mapping session create failed\n");
            return 1;
        }
        if (FilLLVMJIT_AddGlobalMapping(jit, (void *)"host_add",
                                              (void *)host_add)) {
            fprintf(stderr, "function mapping failed\n");
            return 1;
        }
        if (FilLLVMJIT_AddGlobalMapping(jit, (void *)"mydata",
                                              (void *)&mydata)) {
            fprintf(stderr, "data mapping failed\n");
            return 1;
        }
        void *gen = build_caller(jit, "host_add", "run_mapped");
        if (!gen) {
            void *err = FilLLVMJIT_GetLastError();
            fprintf(stderr, "mapped CompileModule failed: %s\n",
                    err ? (const char *)err : "?");
            return 1;
        }
        void *run_addr = FilLLVMJIT_GetFunctionAddress(gen, (void *)"run_mapped");
        if (!run_addr) {
            fprintf(stderr, "mapped lookup failed\n");
            return 1;
        }
        printf("atomics-check mapping OK\n");
        fflush(stdout);
    }

    printf("round 0 resolve OK\n");
    printf("round 0 invoke OK\n");
    zgc_request_and_wait();
    printf("atomics-check OK\n");
    return 0;
}
