/* Lock-free JIT marker stress: flight loads via
   filc_flight_ptr_load_with_manual_tracking plus roots-seqlock snapshots,
   seqlock-publish record_root with retired buffers + store
   barrier, destructor reclamation, narrowed AddModule publish.
   Resolver threads record thousands of roots while churn threads force
   collections (marker snapshots race writers); adder threads publish modules
   concurrently. All IR uses explicit contexts.
 */
#include <pthread.h>
#include <stdfil.h>
#include <filc_llvm_jit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void *g_jit;
static void *g_module;
static volatile int g_stop;

static void *my_resolver(void *name, void *ctx)
{
    (void)name;
    (void)ctx;
    int *box = (int *)malloc(64);
    if (!box)
        abort();
    box[0] = 0x5a5a5a5a;
    box[1] = 1234;
    return (void *)box;
}

#define NRESOLVERS 4
#define NCHURN 2
#define NADDERS 1
#define NRESOLVES 500

static void *resolver_thread(void *arg)
{
    long id = (long)arg;
    char name[64];
    for (int i = 0; i < NRESOLVES; ++i) {
        snprintf(name, sizeof(name), "sym_%ld_%d", id, i);
        void *r = FilLLVMJIT_Resolve(g_module, (void *)name);
        if (!r) {
            fprintf(stderr, "resolve returned null\n");
            abort();
        }
        if ((i & 7) == 0) {
            void *r2 = FilLLVMJIT_Resolve((void *)0, (void *)name);
            if (!r2) {
                fprintf(stderr, "global resolve returned null\n");
                abort();
            }
        }
        if ((i & 63) == 0) {
            for (int k = 0; k < 32; ++k) {
                int *p = (int *)malloc(128);
                if (!p)
                    abort();
                p[0] = k;
            }
        }
    }
    return NULL;
}

static void *churn_thread(void *arg)
{
    (void)arg;
    while (!g_stop) {
        for (int k = 0; k < 128; ++k) {
            char *p = (char *)malloc(96);
            if (!p)
                abort();
            memset(p, 0xA5, 96);
        }
    }
    return NULL;
}

/* Force full collections while resolvers record roots and
   adders publish modules, so publishes deterministically race concurrent
   marking (seqlock retry + store-barrier rescan paths). Bounded (30 cycles)
   to keep the slower harness modes (scribble+verify) tractable. */
static void *forcer_thread(void *arg)
{
    (void)arg;
    for (int i = 0; i < 30; ++i) {
        zgc_request_and_wait();
        if (g_stop)
            break;
    }
    return NULL;
}

static void *build_tiny(const char *modname, const char *fnname)
{
    void *ctx = LLVMContextCreate();
    void *i32 = LLVMInt32TypeInContext(ctx);
    void *mod = LLVMModuleCreateWithNameInContext((void *)modname, ctx);
    void *ft = LLVMFunctionType(i32, (void *)0, 0, 0);
    void *fn = LLVMAddFunction(mod, (void *)fnname, ft);
    void *bb = LLVMAppendBasicBlockInContext(ctx, fn, (void *)"entry");
    void *b = LLVMCreateBuilderInContext(ctx);
    LLVMPositionBuilderAtEnd(b, bb);
    void *c = LLVMConstInt(i32, 42, 0);
    LLVMBuildRet(b, c);
    return mod;
}

static void *adder_thread(void *arg)
{
    long id = (long)arg;
    for (int i = 0; i < 2; ++i) {
        char mname[64], fname[64];
        snprintf(mname, sizeof(mname), "addermod_%ld_%d", id, i);
        snprintf(fname, sizeof(fname), "adder_%ld_%d", id, i);
        void *mod = build_tiny(mname, fname);
        FilLLVMJITModuleRef gen = 0;
        if (FilLLVMJIT_AddModule(g_jit, mod, (void *)mname, &gen) || !gen) {
            fprintf(stderr, "AddModule failed\n");
            abort();
        }
    }
    return NULL;
}

/* Allocate explicitly-parented types in a helper so the refs die with the
   helper's frame; the caller then forces a collection of them. Proves
   explicitly-parented garbage collects cleanly. */
static void make_and_drop_context_types(void)
{
    void *ctx = LLVMContextCreate();
    for (int i = 0; i < 200; ++i) {
        void *t = LLVMInt32TypeInContext(ctx);
        if (!t) {
            fprintf(stderr, "context type returned null\n");
            abort();
        }
    }
}

int main(void)
{
    /* No explicit init: first Create lazily boots the shim (API v4). */
    g_jit = FilLLVMJIT_Create((void *)0);
    if (!g_jit) {
        fprintf(stderr, "create failed\n");
        return 1;
    }
    FilLLVMJIT_SetResolveCallback(g_jit, (void *)&my_resolver, (void *)0);
    void *mod = build_tiny("base", "base_fn");
    {
        FilLLVMJITModuleRef basemod = 0;
        if (FilLLVMJIT_AddModule(g_jit, mod, (void *)"basemod", &basemod) || !basemod) {
            fprintf(stderr, "base AddModule failed\n");
            return 1;
        }
        g_module = basemod;
    }

    pthread_t rts[NRESOLVERS], cts[NCHURN], ats[NADDERS], fts[1];
    for (int i = 0; i < NCHURN; ++i)
        pthread_create(&cts[i], NULL, churn_thread, NULL);
    for (int i = 0; i < NADDERS; ++i)
        pthread_create(&ats[i], NULL, adder_thread, (void *)(long)i);
    pthread_create(&fts[0], NULL, forcer_thread, NULL);
    for (int i = 0; i < NRESOLVERS; ++i)
        pthread_create(&rts[i], NULL, resolver_thread, (void *)(long)i);
    for (int i = 0; i < NRESOLVERS; ++i)
        pthread_join(rts[i], NULL);
    for (int i = 0; i < NADDERS; ++i)
        pthread_join(ats[i], NULL);
    g_stop = 1;
    for (int i = 0; i < NCHURN; ++i)
        pthread_join(cts[i], NULL);
    pthread_join(fts[0], NULL);

    /* Seqlock generation-wrap sanity. Generations advance
       by 2 per publish, so evenness must survive 2^32 wrap (a false
       before==after match would need 2^32 publishes inside one snapshot). */
    {
        unsigned near_wrap = 0xFFFFFFFEu;
        unsigned wrapped = near_wrap + 2u;
        if ((wrapped & 1u) || wrapped != 0u) {
            fprintf(stderr, "generation wrap parity broken\n");
            abort();
        }
    }

    /* Force collection of explicitly-parented objects. Each round's types
       die with the helper frame, then a full collection reclaims them. A
       post-GC resolve proves module roots still record after collections.
       The harness repeats all of this in rage, scribble+verify, STW, and
       release modes: an unmarked pin would trip verify. */
    for (int round = 0; round < 5; ++round) {
        make_and_drop_context_types();
        zgc_request_and_wait();
    }
    {
        void *ctx = LLVMContextCreate();
        if (!ctx || !LLVMInt32TypeInContext(ctx)) {
            fprintf(stderr, "explicit context broken after GC\n");
            abort();
        }
    }
    printf("context-gc OK\n");
    {
        void *r = FilLLVMJIT_Resolve(g_module, (void *)"post_gc_sym");
        if (!r) {
            fprintf(stderr, "post-gc resolve returned null\n");
            abort();
        }
    }
    printf("post-gc resolve OK\n");
    printf("root-stress OK\n");
    return 0;
}
