/* Re-entrancy: nested compile + GC while a JITed frame is live. Every other
   GC/compile in this suite happens on the host with no JITed frame below it;
   this test runs both under a live JITed frame.

   Shape: the host maps two callbacks into module A and calls the JITed
   outer(arr, n). outer allocates a zgc buffer via the mapped hostAlloc (so
   after hostAlloc returns, that buffer is referenced ONLY from JITed state —
   registers/spill slots of the JITed frame), stores into arr, then calls the
   mapped nested_host. nested_host builds + compiles module B with its own
   fresh session/context (a nested compile under the live outer frame), calls
   B's function, forces GC with zgc_request_and_wait() plus allocation churn,
   and calls B's function again — all while outer's frame holds arr, n, the
   running accumulator, and buf. outer then keeps using all of that state
   after nested_host returns: it reads/writes arr, reads buf (if the
   collector failed to account for the JITed frame, buf would have been
   swept and reused, so this access panics or reads garbage — deterministic
   either way), and returns an accumulator derived from pre-call state, the
   nested result, and post-call state. The host re-checks arr's contents from
   its own reference after the call.

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
#define GCS_PER_NESTED 8

/* Mapped into module A. Returns a fresh zgc-allocated array filled with a
   known pattern; the returned pointer ends up held only by the JITed frame
   across the GC that nested_host forces. */
static __attribute__((noinline)) int* hostAlloc(int n)
{
    int* p = (int*)zgc_alloc(sizeof(int) * (size_t)n);
    int i;
    for (i = 0; i < n; ++i)
        p[i] = 11 * i;
    return p;
}

/* Builds + compiles module B (function f(y) = y*2 + x, x baked in) in its own
   fresh session/context — a nested compile while module A's JITed frame is
   live below the host. Each session/context/module here is independent of
   the outer one. */
static __attribute__((noinline)) int (*buildB(int x))(int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("testB", context);
    LLVMTypeRef paramTypes[1] = { LLVMInt32TypeInContext(context) };
    LLVMTypeRef functionType = LLVMFunctionType(
        LLVMInt32TypeInContext(context), paramTypes, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "bFunc", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef result = LLVMBuildAdd(
        builder,
        LLVMBuildMul(builder, LLVMGetParam(function, 0),
                     LLVMConstInt(LLVMInt32TypeInContext(context), 2, false), "d"),
        LLVMConstInt(LLVMInt32TypeInContext(context), (unsigned)x, false), "r");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*bFunc)(int) = FilLLVMJIT_GetPointerToGlobal(compiled, "bFunc");
    return bFunc;
}

/* Mapped into module A. Runs with module A's JITed frame live on the stack:
   compiles module B, calls it, forces GC with churn, calls it again, and
   returns a combined value. */
static __attribute__((noinline)) int nested_host(int x)
{
    int (*bFunc)(int) = buildB(x);
    int first = bFunc(10);
    unsigned k;
    for (k = 0; k < GCS_PER_NESTED; ++k) {
        int* churn = (int*)zgc_alloc(8192);
        churn[k] = (int)k;
        (void)churn;
        zgc_request_and_wait();
    }
    int second = bFunc(20);
    return first + second + x;
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
    LLVMTypeRef nestedType = LLVMFunctionType(i32Type, &i32Type, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMValueRef hostAllocValue = LLVMAddFunction(module, "hostAlloc", hostAllocType);
    LLVMValueRef nestedValue = LLVMAddFunction(module, "nested_host", nestedType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);

    LLVMValueRef a = LLVMGetParam(function, 0);
    LLVMValueRef n = LLVMGetParam(function, 1);

    /* buf comes back from the mapped host allocator; from here on the JITed
       frame is its only holder until the post-GC reads below. */
    LLVMValueRef buf = LLVMBuildCall2(
        builder, hostAllocType, hostAllocValue, &n, 1, "buf");

    LLVMValueRef sum = LLVMConstInt(i32Type, 0, false);
    int i;
    /* Pre-call phase: derive and store, accumulating live state. */
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
    /* The mid-function call: inside it, a fresh session compiles module B and
       the collector runs repeatedly while this frame holds a, n, buf, sum. */
    LLVMValueRef nestedArgs[1] = {
        LLVMConstInt(i32Type, 5, false)
    };
    LLVMValueRef r = LLVMBuildCall2(
        builder, nestedType, nestedValue, nestedArgs, 1, "r");
    sum = LLVMBuildAdd(builder, sum, r, "sum");
    /* Post-call phase: the JITed frame's locals must all still be intact. */
    for (i = 0; i < ELEMENTS; ++i) {
        LLVMValueRef idx = LLVMConstInt(i32Type, (unsigned)i, false);
        LLVMValueRef elemPtr = LLVMBuildInBoundsGEP2(builder, i32Type, a, &idx, 1, "p");
        LLVMValueRef u = LLVMBuildLoad2(builder, i32Type, elemPtr, "u");
        LLVMValueRef w = LLVMBuildAdd(builder, u, r, "w");
        LLVMBuildStore(builder, w, elemPtr);
        sum = LLVMBuildAdd(builder, sum, u, "sum");
    }
    /* Reading buf after the GC: this is the load-bearing liveness check —
       buf is referenced only from JITed state across the GC. */
    for (i = 0; i < ELEMENTS; ++i) {
        LLVMValueRef idx = LLVMConstInt(i32Type, (unsigned)i, false);
        LLVMValueRef bufPtr = LLVMBuildInBoundsGEP2(
            builder, i32Type, buf, &idx, 1, "bp");
        LLVMValueRef b = LLVMBuildLoad2(builder, i32Type, bufPtr, "b");
        sum = LLVMBuildAdd(builder, sum, b, "sum");
    }
    sum = LLVMBuildAdd(builder, sum, n, "sum");
    LLVMBuildRet(builder, sum);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_SetGlobalMapping(jit, "hostAlloc", hostAlloc);
    FilLLVMJIT_SetGlobalMapping(jit, "nested_host", nested_host);
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

        /* nested_host(5): bFunc(10) + bFunc(20) + 5 with f(y) = y*2 + 5. */
        int nested = (10 * 2 + 5) + (20 * 2 + 5) + 5;
        int want = 0;
        for (i = 0; i < ELEMENTS; ++i)
            want += arr0[i] + i + 1; /* pre-call phase accumulator */
        want += want;                /* post-call phase re-reads those values */
        want += nested;              /* the mid-call result */
        for (i = 0; i < ELEMENTS; ++i)
            want += 11 * i;          /* the buf pattern, read after the GC */
        want += ELEMENTS;            /* n, used after the call */

        if (r != want) {
            fprintf(stderr, "round %d: outer returned %d, expected %d\n",
                    round, r, want);
            return 1;
        }
        /* The host's own reference must see exactly what the JITed frame
           wrote before and after the GC it survived. */
        for (i = 0; i < ELEMENTS; ++i) {
            if (arr[i] != arr0[i] + i + 1 + nested) {
                fprintf(stderr, "round %d: arr[%d] is %d, expected %d\n",
                        round, i, arr[i], arr0[i] + i + 1 + nested);
                return 1;
            }
        }
    }

    printf("gc nested compile ok\n");
    return 0;
}
