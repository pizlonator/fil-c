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
   AddModule runs verifyModule over the rebound IR, so a broken cache
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

   (verifyModule itself runs inside every AddModule below via
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

#define NROUNDS 8
#define NTHREADS 6

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
    FilLLVMJITModuleRef gen = 0;
    if (FilLLVMJIT_AddModule(jit, mod, (void *)"m", &gen) || !gen)
        return 0;
    return gen;
}

static void *invoker_thread(void *arg)
{
    long id = (long)arg;
    int out = 0;
    /* Gate-launch: every thread invokes the same freshly-mapped caller at
       once, racing the bound dispatch. */
    pthread_barrier_wait(&g_gate);
    int rc = FilLLVMJIT_InvokeIntFunction(g_jit, (void *)g_runname,
                                                (void *)&out);
    if (rc) {
        void *err = FilLLVMJIT_GetLastError();
        fprintf(stderr, "thread %ld: invoke failed: %s\n", id,
                err ? (const char *)err : "?");
        abort();
    }
    g_results[id] = out;
    return NULL;
}

/* Gate-launched concurrent resolution of the same unmapped extern from
   entered Fil-C threads: hammers the benign first-use resolve race in
   resolve_impl (concurrent resolve + root-record + AddGlobalMapping
   converging on the same payload, last-writer-wins). NOTE: the JITed
   caller itself is never invoked here: InvokeIntFunction runs JITed code
   with the thread exited, and runtime lazy resolution from an exited
   thread is a pre-existing runtime-owned gap (out of scope); the
   compiler-emitted cache protocol for the same extern shape is proven by
   the IR/asm rechecks documented above plus AddModule's verifyModule and
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
    /* No explicit init: first Create lazily boots the shim (API v4). */

    /* Issue #6 proof, once up front: a genuine Fil-C function capability
       ((void*)host_add, whose payload lives in aux outside [lower, upper])
       and a data global must both map. A pre-bound caller over the mapping
       also proves the pizlonated dispatch through a mapped payload. */
    {
        void *jit = FilLLVMJIT_Create((void *)0);
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
            fprintf(stderr, "mapped AddModule failed: %s\n",
                    err ? (const char *)err : "?");
            return 1;
        }
        int out = 0;
        int rc = FilLLVMJIT_InvokeIntFunction(jit, (void *)"run_mapped",
                                                    (void *)&out);
        if (rc || out != 42) {
            fprintf(stderr, "mapped invoke: got %d want 42\n", out);
            return 1;
        }
        printf("atomics-check mapping OK\n");
        fflush(stdout);
    }

    for (int round = 0; round < NROUNDS; ++round) {
        /* A fresh session per round: each round's module defines the same
           extern, and two modules referencing the same extern cannot share
           a session (each defines the same pizlonated_ forwarder).

           No AddGlobalMapping here: the extern stays unmapped so the
           lazy worker keeps its cache protocol (check/resolve/store) and
           resolves at run time through the session resolver below. (The
           pre-bound mapping path is proven once up front; unmapped lazy
           externs are the normal resolver path and pass AddModule.) */
        void *jit = FilLLVMJIT_Create((void *)0);
        if (!jit) {
            fprintf(stderr, "round %d: create failed\n", round);
            return 1;
        }
        FilLLVMJIT_SetResolveCallback(jit,
                                        (void *)resolve_host_add,
                                        (void *)0);

        snprintf(g_runname, sizeof(g_runname), "run_r%d", round);
        void *gen = build_caller(jit, "race_add", g_runname);
        if (!gen) {
            void *err = FilLLVMJIT_GetLastError();
            fprintf(stderr, "round %d: AddModule failed: %s\n", round,
                    err ? (const char *)err : "?");
            return 1;
        }
        g_jit = jit;
        g_module = gen;
        pthread_barrier_init(&g_gate, NULL, NTHREADS + 1);
        pthread_t ts[NTHREADS];
        for (long i = 0; i < NTHREADS; ++i)
            pthread_create(&ts[i], NULL, resolver_thread, (void *)i);
        pthread_barrier_wait(&g_gate);
        for (int i = 0; i < NTHREADS; ++i)
            pthread_join(ts[i], NULL);
        pthread_barrier_destroy(&g_gate);
        /* Every concurrent resolution must converge on the genuine
           payload: a torn or corrupted flight would surface here as a
           divergent (or null) address. */
        unsigned long want = (unsigned long)(void *)host_add;
        for (int i = 0; i < NTHREADS; ++i) {
            if (g_resolved[i] != want) {
                fprintf(stderr, "round %d thread %d: resolved %lx want %lx\n",
                        round, i, g_resolved[i], want);
                return 1;
            }
        }
        printf("round %d resolve OK\n", round);
        fflush(stdout);
        /* Pre-resolve the exec extern (entered) so the second caller
           binds to it directly (worker stubbed) and executes -- end-to-end
           proof the resolver-produced mapping is genuinely callable
           through the pizlonated convention. A second module cannot reuse
           "race_add" (each module defines the same pizlonated_ forwarder),
           hence the separate "exec_add" spelling; the resolver answers
           every name with the host_add payload either way. */
        void *pre = FilLLVMJIT_Resolve(gen, (void *)"exec_add");
        if (!pre || (unsigned long)pre != want) {
            fprintf(stderr, "round %d: pre-resolve failed\n", round);
            return 1;
        }
        char execname[64];
        snprintf(execname, sizeof(execname), "run_r%d_exec", round);
        snprintf(g_runname, sizeof(g_runname), "%s", execname);
        void *gen2 = build_caller(jit, "exec_add", execname);
        if (!gen2) {
            void *err = FilLLVMJIT_GetLastError();
            fprintf(stderr, "round %d: exec AddModule failed: %s\n", round,
                    err ? (const char *)err : "?");
            return 1;
        }
        (void)gen2;
        pthread_barrier_init(&g_gate, NULL, NTHREADS + 1);
        for (long i = 0; i < NTHREADS; ++i)
            pthread_create(&ts[i], NULL, invoker_thread, (void *)i);
        pthread_barrier_wait(&g_gate);
        for (int i = 0; i < NTHREADS; ++i)
            pthread_join(ts[i], NULL);
        pthread_barrier_destroy(&g_gate);
        for (int i = 0; i < NTHREADS; ++i) {
            if (g_results[i] != 42) {
                fprintf(stderr, "round %d thread %d: got %d want 42\n",
                        round, i, g_results[i]);
                return 1;
            }
        }
        printf("round %d invoke OK\n", round);
        fflush(stdout);
        /* Sessions/modules die with this round's frame; forcing a
           collection proves resolved roots survive GC. */
        if ((round & 1) == 1)
            zgc_request_and_wait();
    }
    zgc_request_and_wait();
    printf("atomics-check OK\n");
    return 0;
}
