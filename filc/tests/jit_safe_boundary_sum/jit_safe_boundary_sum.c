/* In-bounds memory access through JITed code: the host passes an array and a
   length; the JITed function sums the first n elements. Every access is in
   bounds (the array always has ELEMENTS elements; n only masks which values
   enter the sum), so no panic — just the right answer. The length is laundered
   through opaque() so the JITed bounds checks stay honest. Both a full-length
   call (n == ELEMENTS) and a partial call (n < ELEMENTS) are checked, so the
   masked select has to actually mask.
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

#define ELEMENTS 10

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
        /* The false arm must be a zero constant, not the running sum: when
           n <= i the masked value is added to sum, so selecting sum here
           would double the sum on every out-of-range iteration. */
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
        arr[i] = i * i;

    /* Full-length call: every element enters the sum. */
    int n = (int)(intptr_t)opaque((void*)(intptr_t)ELEMENTS);
    int r = test((int*)opaque(arr), n);
    int want = 0;
    for (i = 0; i < ELEMENTS; ++i)
        want += i * i;
    if (r != want) {
        fprintf(stderr, "full sum %d, expected %d\n", r, want);
        return 1;
    }

    /* Partial call: only the first n elements enter the sum, so the select's
       zero arm must actually mask the rest (a select whose false arm is the
       running sum would double the sum on every masked iteration). */
    n = (int)(intptr_t)opaque((void*)(intptr_t)3);
    r = test((int*)opaque(arr), n);
    want = 0;
    for (i = 0; i < n; ++i)
        want += i * i;
    if (r != want) {
        fprintf(stderr, "partial sum %d, expected %d\n", r, want);
        return 1;
    }

    printf("safe sum ok\n");
    return 0;
}

