/* Re-entrancy: repeated GC under a live JITed frame. This is the pure
   GC-under-JITed-frame stress (the combined nested-compile + GC scenario is
   jit_gc_nested_compile): the JITed outer(arr, n) holds live state — the arr
   parameter, n, a running accumulator, and a zgc buffer returned by the
   mapped hostAlloc that is referenced ONLY from JITed state — across three
   separate calls into the mapped churn_host, each of which forces multiple
   collections with zgc_request_and_wait() plus allocation churn. After every
   callback the JITed code keeps reading and writing arr and (after the GCs)
   buf, and the returned accumulator is derived from state from every phase.
   If the collector failed to account for the JITed frame, the buf accesses
   after the first churn_host would panic or read garbage — deterministic
   either way. The host re-checks arr's contents from its own reference after
   the call.

   Module A has no IR loops (JITed back-edges are not yet supported — see
   jit_bogus_loop_backedge), so the element loop is unrolled. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

#define ELEMENTS 4
#define ROUNDS 3
#define GCS_PER_CALLBACK 4

/* Mapped into module A. Returns a fresh zgc-allocated array filled with a
   known pattern; the returned pointer is held only by the JITed frame across
   all the GCs below. */
static __attribute__((noinline)) int* hostAlloc(int n)
{
    int* p = (int*)zgc_alloc(sizeof(int) * (size_t)n);
    int i;
    for (i = 0; i < n; ++i)
        p[i] = 11 * i;
    return p;
}

/* Mapped into module A. Forces GCS_PER_CALLBACK collections (with churn)
   while module A's JITed frame is live on the stack, then returns a value
   derived from k. */
static __attribute__((noinline)) int churn_host(int k)
{
    int j;
    for (j = 0; j < GCS_PER_CALLBACK; ++j) {
        int* churn = (int*)zgc_alloc(256 * 1024);
        churn[j] = k + j;
        (void)churn;
        zgc_request_and_wait();
    }
    return 100 * k;
}

static FilLLVMJITModuleRef aModule; /* kept alive on purpose */

static __attribute__((noinline)) int (*buildOuter(void))(int*, int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("testA", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[2] = { ptrType, i32Type };
    LLVMTypeRef functionType = LLVMFunctionType(i32Type, paramTypes, 2, false);
    LLVMTypeRef hostAllocType = LLVMFunctionType(ptrType, &i32Type, 1, false);
    LLVMTypeRef churnType = LLVMFunctionType(i32Type, &i32Type, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMValueRef hostAllocValue = LLVMAddFunction(module, "hostAlloc", hostAllocType);
    LLVMValueRef churnValue = LLVMAddFunction(module, "churn_host", churnType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);

    LLVMValueRef a = LLVMGetParam(function, 0);
    LLVMValueRef n = LLVMGetParam(function, 1);

    LLVMValueRef buf = LLVMBuildCall2(
        builder, hostAllocType, hostAllocValue, &n, 1, "buf");

    LLVMValueRef sum = LLVMConstInt(i32Type, 0, false);
    int i;

    /* Phase 1: derive and store; the accumulator holds live state. */
    for (i = 0; i < ELEMENTS; ++i) {
        LLVMValueRef idx = LLVMConstInt(i32Type, (unsigned)i, false);
        LLVMValueRef elemPtr = LLVMBuildInBoundsGEP2(builder, i32Type, a, &idx, 1, "p");
        LLVMValueRef v = LLVMBuildLoad2(builder, i32Type, elemPtr, "v");
        LLVMValueRef w = LLVMBuildAdd(
            builder, v,
            LLVMConstInt(i32Type, (unsigned)(i + 1), false), "w");
        LLVMBuildStore(builder, w, elemPtr);
        sum = LLVMBuildAdd(builder, sum, w, "sum");
    }

    /* GC #1 under this frame. */
    LLVMValueRef args1[1] = { LLVMConstInt(i32Type, 1, false) };
    LLVMValueRef r1 = LLVMBuildCall2(builder, churnType, churnValue, args1, 1, "r1");
    sum = LLVMBuildAdd(builder, sum, r1, "sum");

    /* Phase 2: pre-call stores survived the GC; re-read and re-write. */
    for (i = 0; i < ELEMENTS; ++i) {
        LLVMValueRef idx = LLVMConstInt(i32Type, (unsigned)i, false);
        LLVMValueRef elemPtr = LLVMBuildInBoundsGEP2(builder, i32Type, a, &idx, 1, "p");
        LLVMValueRef u = LLVMBuildLoad2(builder, i32Type, elemPtr, "u");
        LLVMBuildStore(builder, LLVMBuildAdd(builder, u, r1, "w"), elemPtr);
        sum = LLVMBuildAdd(builder, sum, u, "sum");
    }

    /* GC #2 under this frame — buf is referenced only from JITed state. */
    LLVMValueRef args2[1] = { LLVMConstInt(i32Type, 2, false) };
    LLVMValueRef r2 = LLVMBuildCall2(builder, churnType, churnValue, args2, 1, "r2");
    sum = LLVMBuildAdd(builder, sum, r2, "sum");

    /* Phase 3: read and even re-write buf after two rounds of GC. */
    for (i = 0; i < ELEMENTS; ++i) {
        LLVMValueRef idx = LLVMConstInt(i32Type, (unsigned)i, false);
        LLVMValueRef bufPtr = LLVMBuildInBoundsGEP2(
            builder, i32Type, buf, &idx, 1, "bp");
        LLVMValueRef b = LLVMBuildLoad2(builder, i32Type, bufPtr, "b");
        LLVMBuildStore(builder, LLVMBuildAdd(builder, b, r2, "w"), bufPtr);
        sum = LLVMBuildAdd(builder, sum, b, "sum");
    }

    /* GC #3 under this frame — buf and arr both live only in JITed state
       relative to the callback (arr is also host-rooted). */
    LLVMValueRef args3[1] = { LLVMConstInt(i32Type, 3, false) };
    LLVMValueRef r3 = LLVMBuildCall2(builder, churnType, churnValue, args3, 1, "r3");
    sum = LLVMBuildAdd(builder, sum, r3, "sum");

    /* Phase 4: everything must still be intact; combine and store once more. */
    for (i = 0; i < ELEMENTS; ++i) {
        LLVMValueRef idx = LLVMConstInt(i32Type, (unsigned)i, false);
        LLVMValueRef elemPtr = LLVMBuildInBoundsGEP2(builder, i32Type, a, &idx, 1, "p");
        LLVMValueRef bufPtr = LLVMBuildInBoundsGEP2(
            builder, i32Type, buf, &idx, 1, "bp");
        LLVMValueRef v = LLVMBuildLoad2(builder, i32Type, elemPtr, "v");
        LLVMValueRef b = LLVMBuildLoad2(builder, i32Type, bufPtr, "b");
        LLVMBuildStore(builder, LLVMBuildAdd(builder, v, b, "w"), elemPtr);
        sum = LLVMBuildAdd(builder, sum, v, "sum");
        sum = LLVMBuildAdd(builder, sum, b, "sum");
    }

    sum = LLVMBuildAdd(builder, sum, n, "sum");
    LLVMBuildRet(builder, sum);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_SetGlobalMapping(jit, "hostAlloc", hostAlloc);
    FilLLVMJIT_SetGlobalMapping(jit, "churn_host", churn_host);
    aModule = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int*, int) = FilLLVMJIT_GetPointerToGlobal(aModule, "test");
    return test;
}

int main()
{
    int (*outer)(int*, int) = buildOuter();
    int arr0[ELEMENTS];
    int round;
    for (round = 0; round < ROUNDS; ++round) {
        int i;
        for (i = 0; i < ELEMENTS; ++i)
            arr0[i] = i * 3 + round;
        int* arr = (int*)zgc_alloc(sizeof(int) * (size_t)ELEMENTS);
        for (i = 0; i < ELEMENTS; ++i)
            arr[i] = arr0[i];

        int n = (int)(intptr_t)opaque((void*)(intptr_t)ELEMENTS);
        int r = outer((int*)opaque(arr), n);

        /* Mirror the JITed phases: r1 = 100, r2 = 200, r3 = 300. */
        int phase[ELEMENTS];
        int want = 0;
        for (i = 0; i < ELEMENTS; ++i) {
            phase[i] = arr0[i] + i + 1; /* phase 1 store */
            want += phase[i];
        }
        want += 100; /* r1 */
        for (i = 0; i < ELEMENTS; ++i)
            want += phase[i]; /* phase 2 re-read (pre-GC values) */
        want += 200; /* r2 */
        for (i = 0; i < ELEMENTS; ++i)
            want += 11 * i; /* phase 3 buf re-read */
        want += 300; /* r3 */
        for (i = 0; i < ELEMENTS; ++i)
            want += (phase[i] + 100) + (11 * i + 200); /* phase 4 re-reads */
        want += ELEMENTS; /* n */

        if (r != want) {
            fprintf(stderr, "round %d: outer returned %d, expected %d\n",
                    round, r, want);
            return 1;
        }
        for (i = 0; i < ELEMENTS; ++i) {
            int expected = (arr0[i] + i + 1 + 100) + (11 * i + 200);
            if (arr[i] != expected) {
                fprintf(stderr, "round %d: arr[%d] is %d, expected %d\n",
                        round, i, arr[i], expected);
                return 1;
            }
        }
    }

    printf("gc under jitted frame ok\n");
    return 0;
}
