/* 10,000 calls into one JITed function with varying in-bounds lengths: the
   function sums the first n elements of a 16-int array, and the host checks
   the closed-form expectation for every call. Repeated JITed calls must not
   corrupt any state.
   NOTE: the sum is unrolled (with selects for the n masking) because JITed
   code cannot contain IR loops yet — loop back-edges need a
   filc_pollcheck_slow safepoint that the JIT does not currently provide
   ("JIT link failed: unresolved external symbol: filc_pollcheck_slow"). */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

#define CALLS 10000
#define ELEMENTS 16

static __attribute__((noinline)) int (*buildSumFn(void))(int*, int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[2] = { ptrType, i32Type };
    LLVMTypeRef functionType = LLVMFunctionType(i32Type, paramTypes, 2, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef a = LLVMGetParam(function, 0);
    LLVMValueRef n = LLVMGetParam(function, 1);
    LLVMValueRef sum = LLVMConstInt(i32Type, 0, false);
    LLVMValueRef zero = LLVMConstInt(i32Type, 0, false);
    int i;
    for (i = 0; i < ELEMENTS; ++i) {
        LLVMValueRef idx = LLVMConstInt(i32Type, (unsigned)i, false);
        LLVMValueRef elemPtr = LLVMBuildInBoundsGEP2(builder, i32Type, a, &idx, 1, "p");
        LLVMValueRef v = LLVMBuildLoad2(builder, i32Type, elemPtr, "v");
        LLVMValueRef inRange = LLVMBuildICmp(
            builder, LLVMIntSGT, n, LLVMConstInt(i32Type, (unsigned)i, false), "inrange");
        LLVMValueRef masked = LLVMBuildSelect(builder, inRange, v, zero, "masked");
        sum = LLVMBuildAdd(builder, sum, masked, "sum");
    }
    LLVMBuildRet(builder, sum);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int*, int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    int (*test)(int*, int) = buildSumFn();
    int arr[ELEMENTS];
    int i;
    for (i = 0; i < ELEMENTS; ++i)
        arr[i] = i;

    for (i = 0; i < CALLS; ++i) {
        int n = (int)(intptr_t)opaque((void*)(intptr_t)((size_t)(i * 7) % (ELEMENTS + 1)));
        int r = test((int*)opaque(arr), n);
        int want = n * (n - 1) / 2;
        if (r != want) {
            fprintf(stderr, "call %d (n = %d) returned %d, expected %d\n",
                    i, n, r, want);
            return 1;
        }
    }

    printf("safe many ok\n");
    return 0;
}

