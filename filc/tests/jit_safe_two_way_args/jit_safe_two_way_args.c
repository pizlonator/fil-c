/* Two-way arguments: the JITed function receives both a source and a
   destination array plus a length, and copies n elements. All accesses are
   in bounds; the host verifies the destination afterwards.
   NOTE: the copy is unrolled (with selects for the n masking) because JITed
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

#define ELEMENTS 5

static __attribute__((noinline)) void (*buildFn(void))(int*, int*, int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[3] = { ptrType, ptrType, i32Type };
    LLVMTypeRef functionType = LLVMFunctionType(
        LLVMVoidTypeInContext(context), paramTypes, 3, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef in = LLVMGetParam(function, 0);
    LLVMValueRef out = LLVMGetParam(function, 1);
    LLVMValueRef n = LLVMGetParam(function, 2);
    int i;
    for (i = 0; i < ELEMENTS; ++i) {
        LLVMValueRef idx = LLVMConstInt(i32Type, (unsigned)i, false);
        LLVMValueRef srcPtr = LLVMBuildInBoundsGEP2(builder, i32Type, in, &idx, 1, "sp");
        LLVMValueRef dstPtr = LLVMBuildInBoundsGEP2(builder, i32Type, out, &idx, 1, "dp");
        LLVMValueRef v = LLVMBuildLoad2(builder, i32Type, srcPtr, "v");
        LLVMValueRef old = LLVMBuildLoad2(builder, i32Type, dstPtr, "old");
        LLVMValueRef inRange = LLVMBuildICmp(
            builder, LLVMIntSGT, n, LLVMConstInt(i32Type, (unsigned)i, false), "inrange");
        LLVMValueRef picked = LLVMBuildSelect(builder, inRange, v, old, "picked");
        LLVMBuildStore(builder, picked, dstPtr);
    }
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    void (*test)(int*, int*, int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    void (*test)(int*, int*, int) = buildFn();
    int in[5];
    int out[5];
    int i;
    for (i = 0; i < 5; ++i) {
        in[i] = i * 11 + 3;
        out[i] = -1;
    }
    int n = (int)(intptr_t)opaque((void*)(intptr_t)5);
    test((int*)opaque(in), (int*)opaque(out), n);
    for (i = 0; i < 5; ++i) {
        if (out[i] != in[i]) {
            fprintf(stderr, "out[%d] = %d, expected %d\n", i, out[i], in[i]);
            return 1;
        }
    }

    printf("safe two way ok\n");
    return 0;
}
