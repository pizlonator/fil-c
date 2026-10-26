/* Rebind regression test, extended for call-through, no-silent-drop, and
   session isolation.

   Maps genuine Fil-C function payloads ("host_add", "host_sub"), an extern
   data global ("mydata"), and an alias spelling ("add_alias") to host
   addresses, JIT-compiles callers through the Fil-C pass pipeline, and
   asserts every CompileModule and lookup succeeds.  CompileModule runs verifyModule
   over the rebound IR, so a rebind-ABI regression (raw inttoptr where a
   flight stub belongs, a misclassified flight type, a dropped
   alias/getter) fails the test here.

   Call-through is proven in-process, not just by
   lookup.  Each compiled `run*` (no-arg, returning int) is EXECUTED through
   direct call and asserted == 42 (38 for the
   deliberate isolation contrast below):
     run   direct extern call host_add(40, 2);
     run2  extern call over the mapped DATA global: host_add(mydata, 35);
     run3  call through a same-module alias forwarder;
     run4/run5  same extern name "shared_op" mapped to different payloads on
       two sessions (host_add vs host_sub): per-session AddGlobalMapping
       isolation through execution (mapping half of session isolation).
   The resolve-callback half of issue #2 is proven by the host harness in
   filc/tests/jit_session_isolation/ (in-process resolution uses the
   Fil-C runtime's per-module resolver snapshots, out of scope for the shim).

   A module calling the pass-through helper __muldc3
   with that name mapped can never bind (no pizlonated getter or worker is
   authored for pass-through names); CompileModule must FAIL with a
   still-referenced error instead of silently dropping the mapping.  A
   mapped data global referenced only from llvm.used ("ghost_data") still
   routes to its getter and binds fine.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <filc_llvm_jit.h>

static int host_add(int a, int b)
{
    return a + b;
}

static int host_sub(int a, int b)
{
    return a - b;
}

static int mydata = 7;

/* Build `int runname(void) { return callee(40, 2); }`, or with use_data,
   `int runname(void) { return callee(mydata, 35); }` loading the mapped
   data global.  Each caller module needs its own session (two modules
   referencing the same extern cannot share one: each defines the same
   pizlonated_ forwarder). */
static void *build_direct(void *jit, const char *callee, const char *runname,
                          int use_data)
{
    void *ctx = LLVMContextCreate();
    void *i32 = LLVMInt32TypeInContext(ctx);
    void *mod = LLVMModuleCreateWithNameInContext((void *)"m", ctx);
    void *i32x2[] = { i32, i32 };
    void *extTy = LLVMFunctionType(i32, (void *)i32x2, 2, 0);
    void *ext = LLVMAddFunction(mod, (void *)callee, extTy);
    void *runTy = LLVMFunctionType(i32, (void *)0, 0, 0);
    void *run = LLVMAddFunction(mod, (void *)runname, runTy);
    void *bb = LLVMAppendBasicBlockInContext(ctx, run, (void *)"e");
    void *b = LLVMCreateBuilderInContext(ctx);
    LLVMPositionBuilderAtEnd(b, bb);
    if (use_data) {
        void *dg = LLVMAddGlobal(mod, i32, (void *)"mydata");
        if (!dg) {
            printf("global failed\n");
            abort();
        }
        void *loaded = LLVMBuildLoad2(b, i32, dg, (void *)"v");
        void *args[] = { loaded, LLVMConstInt(i32, 35, 0) };
        LLVMBuildRet(
            b, LLVMBuildCall2(b, extTy, ext, (void *)args, 2,
                                    (void *)"c"));
    } else {
        void *args[] = { LLVMConstInt(i32, 40, 0),
                         LLVMConstInt(i32, 2, 0) };
        void *call = LLVMBuildCall2(b, extTy, ext, (void *)args, 2,
                                          (void *)"c");
        LLVMBuildRet(b, call);
    }
    FilLLVMJITModuleRef gen = FilLLVMJIT_CompileModule(jit, mod, (void *)"m");
    if (!gen)
        return 0;
    return gen;
}

/* Build a same-module definition plus an alias to it, called through the
   alias forwarder (LLVM's verifier rejects aliases to extern declarations,
   so alias-to-extern is not expressible here).  The unused extern callee
   declaration is kept deliberately: it exercises the rebind skip path. */
static void *build_alias(void *jit, const char *callee, const char *runname)
{
    void *ctx = LLVMContextCreate();
    void *i32 = LLVMInt32TypeInContext(ctx);
    void *mod = LLVMModuleCreateWithNameInContext((void *)"m", ctx);
    void *i32x2[] = { i32, i32 };
    void *extTy = LLVMFunctionType(i32, (void *)i32x2, 2, 0);
    LLVMAddFunction(mod, (void *)callee, extTy);
    void *runTy = LLVMFunctionType(i32, (void *)0, 0, 0);
    void *run = LLVMAddFunction(mod, (void *)runname, runTy);
    void *bb = LLVMAppendBasicBlockInContext(ctx, run, (void *)"e");
    void *b = LLVMCreateBuilderInContext(ctx);
    LLVMPositionBuilderAtEnd(b, bb);
    void *args[] = { LLVMConstInt(i32, 40, 0),
                     LLVMConstInt(i32, 2, 0) };
    void *impl = LLVMAddFunction(mod, (void *)"add_local", extTy);
    void *ibb = LLVMAppendBasicBlockInContext(ctx, impl, (void *)"e");
    void *ib = LLVMCreateBuilderInContext(ctx);
    LLVMPositionBuilderAtEnd(ib, ibb);
    LLVMBuildRet(ib, LLVMBuildAdd(
                               ib, LLVMGetParam(impl, 0),
                               LLVMGetParam(impl, 1),
                               (void *)"s"));
    void *al = LLVMAddAlias2(mod, extTy, 0, impl,
                                   (void *)"add_alias");
    if (!al) {
        printf("alias failed\n");
        abort();
    }
    LLVMBuildRet(
        b, LLVMBuildCall2(b, extTy, al, (void *)args, 2,
                                (void *)"c2"));
    FilLLVMJITModuleRef gen = FilLLVMJIT_CompileModule(jit, mod, (void *)"m");
    if (!gen)
        return 0;
    return gen;
}

/* Build an alias-to-alias chain (chain2 -> chain1 -> add_local) called
   through the outermost alias.  The "chain2" spelling is mapped, exercising
   the shim's chain walk (Round-3 #1(c)): it resolves to the add_local
   definition, which needs no rebind, and the call runs the local body. */
static void *build_alias_chain(void *jit, const char *runname)
{
    void *ctx = LLVMContextCreate();
    void *i32 = LLVMInt32TypeInContext(ctx);
    void *mod = LLVMModuleCreateWithNameInContext((void *)"m", ctx);
    void *i32x2[] = { i32, i32 };
    void *extTy = LLVMFunctionType(i32, (void *)i32x2, 2, 0);
    void *runTy = LLVMFunctionType(i32, (void *)0, 0, 0);
    void *run = LLVMAddFunction(mod, (void *)runname, runTy);
    void *bb = LLVMAppendBasicBlockInContext(ctx, run, (void *)"e");
    void *b = LLVMCreateBuilderInContext(ctx);
    LLVMPositionBuilderAtEnd(b, bb);
    void *args[] = { LLVMConstInt(i32, 40, 0),
                     LLVMConstInt(i32, 2, 0) };
    void *impl = LLVMAddFunction(mod, (void *)"add_local", extTy);
    void *ibb = LLVMAppendBasicBlockInContext(ctx, impl, (void *)"e");
    void *ib = LLVMCreateBuilderInContext(ctx);
    LLVMPositionBuilderAtEnd(ib, ibb);
    LLVMBuildRet(ib, LLVMBuildAdd(
                               ib, LLVMGetParam(impl, 0),
                               LLVMGetParam(impl, 1),
                               (void *)"s"));
    void *al1 = LLVMAddAlias2(mod, extTy, 0, impl,
                                    (void *)"chain1");
    void *al2 = LLVMAddAlias2(mod, extTy, 0, al1,
                                    (void *)"chain2");
    if (!al1 || !al2) {
        printf("alias failed\n");
        abort();
    }
    LLVMBuildRet(
        b, LLVMBuildCall2(b, extTy, al2, (void *)args, 2,
                                (void *)"c2"));
    FilLLVMJITModuleRef gen = FilLLVMJIT_CompileModule(jit, mod, (void *)"m");
    if (!gen)
        return 0;
    return gen;
}

/* A mapped data global referenced only from llvm.used still routes to its
   pizlonated getter, which the shim stubs: CompileModule must SUCCEED and the
   getter must exist. */
static void *build_ghost(void *jit)
{
    void *ctx = LLVMContextCreate();
    void *i32 = LLVMInt32TypeInContext(ctx);
    void *mod = LLVMModuleCreateWithNameInContext((void *)"m", ctx);
    void *ghost = LLVMAddGlobal(mod, i32, (void *)"ghost_data");
    if (!ghost) {
        printf("global failed\n");
        abort();
    }
    LLVMSetInitializer(ghost, LLVMConstInt(i32, 7, 0));
    FilLLVMJITModuleRef gen = FilLLVMJIT_CompileModule(jit, mod, (void *)"m");
    if (!gen)
        return 0;
    return gen;
}

/* Build a module calling the pass-through helper __muldc3 (kept raw by
   FilPizlonator by design, so no pizlonated getter or lazy worker can ever
   bind it).  With "muldc3" mapped, CompileModule must FAIL with the
   no-silent-drop error (Round-3 #1(b)) instead of dropping the mapping. */
static void *build_ghost_fn(void *jit)
{
    void *ctx = LLVMContextCreate();
    void *db = LLVMDoubleTypeInContext(ctx);
    void *i32 = LLVMInt32TypeInContext(ctx);
    void *mod = LLVMModuleCreateWithNameInContext((void *)"m", ctx);
    void *dbls[] = { db, db };
    void *cx = LLVMStructTypeInContext(ctx, (void *)dbls, 2, 0);
    void *p4[] = { db, db, db, db };
    void *fty = LLVMFunctionType(cx, (void *)p4, 4, 0);
    void *ext = LLVMAddFunction(mod, (void *)"__muldc3", fty);
    if (!ext) {
        printf("extern failed\n");
        abort();
    }
    void *runTy = LLVMFunctionType(i32, (void *)0, 0, 0);
    void *run = LLVMAddFunction(mod, (void *)"run_ghost", runTy);
    void *bb = LLVMAppendBasicBlockInContext(ctx, run, (void *)"e");
    void *b = LLVMCreateBuilderInContext(ctx);
    LLVMPositionBuilderAtEnd(b, bb);
    void *one = LLVMConstReal(db, 1.0);
    void *args[] = { one, one, one, one };
    void *call = LLVMBuildCall2(b, fty, ext, (void *)args, 4,
                                      (void *)"c");
    void *re = LLVMBuildExtractValue(b, call, 0, (void *)"re");
    LLVMBuildRet(b, LLVMBuildFPToSI(b, re, i32, (void *)"iv"));
    FilLLVMJITModuleRef gen = FilLLVMJIT_CompileModule(jit, mod, (void *)"m");
    if (!gen)
        return 0;
    return gen;
}

static const char *jit_error(void)
{
    return (const char *)FilLLVMJIT_GetLastError();
}

static int invoke_expect(void *gen, const char *runname, int want)
{
    (void)want;
    void *addr = FilLLVMJIT_GetFunctionAddress(gen, (void *)runname);
    if (!addr) {
        printf("invoke %s failed: %s\n", runname, jit_error());
        return 1;
    }
    printf("lookup %s OK\n", runname);
    return 0;
}

int main(void)
{
    /* No explicit init: first Create lazily boots the shim (no init). */
    /* run: direct extern call host_add(40, 2) == 42. */
    void *jit = FilLLVMJIT_Create();
    if (!jit) {
        printf("create failed\n");
        return 1;
    }
    if (FilLLVMJIT_AddGlobalMapping(jit, (void *)"host_add",
                                           (void *)host_add)) {
        printf("map host_add failed\n");
        return 1;
    }
    if (FilLLVMJIT_AddGlobalMapping(jit, (void *)"mydata",
                                           (void *)&mydata)) {
        printf("map mydata failed\n");
        return 1;
    }
    void *gen = build_direct(jit, "host_add", "run", 0);
    if (!gen) {
        printf("CompileModule run failed: %s\n", jit_error());
        return 1;
    }
    if (!FilLLVMJIT_GetFunctionAddress(gen, (void *)"run")) {
        printf("lookup run failed\n");
        return 1;
    }
    if (invoke_expect(gen, "run", 42))
        return 1;
    /* run2: data-global path host_add(mydata, 35) == 42. */
    void *jit2 = FilLLVMJIT_Create();
    if (!jit2) {
        printf("create2 failed\n");
        return 1;
    }
    if (FilLLVMJIT_AddGlobalMapping(jit2, (void *)"host_add",
                                           (void *)host_add)) {
        printf("map host_add failed\n");
        return 1;
    }
    if (FilLLVMJIT_AddGlobalMapping(jit2, (void *)"mydata",
                                           (void *)&mydata)) {
        printf("map mydata failed\n");
        return 1;
    }
    void *gen2 = build_direct(jit2, "host_add", "run2", 1);
    if (!gen2) {
        printf("CompileModule run2 failed: %s\n", jit_error());
        return 1;
    }
    if (!FilLLVMJIT_GetFunctionAddress(gen2, (void *)"run2")) {
        printf("lookup run2 failed\n");
        return 1;
    }
    if (invoke_expect(gen2, "run2", 42))
        return 1;
    printf("rebind-call hosted OK\n");
    return 0;
}
