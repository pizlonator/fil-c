/* In-bounds access to the JITed function's own stack: the function allocas an
   int[4], stores through parameter-derived indices 0..3, then loads all four
   back and sums them. The base index is a parameter (opaque-laundered by the
   host), so the checks are real, and every access is in bounds. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

static __attribute__((noinline)) int (*buildFn(void))(int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef paramTypes[1] = { i32Type };
    LLVMTypeRef functionType = LLVMFunctionType(i32Type, paramTypes, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMTypeRef arrayType = LLVMArrayType(i32Type, 4);
    LLVMValueRef buf = LLVMBuildAlloca(builder, arrayType, "buf");
    LLVMValueRef base = LLVMGetParam(function, 0);
    LLVMValueRef zero = LLVMConstInt(i32Type, 0, false);

    LLVMValueRef ptrs[4];
    LLVMValueRef sum = zero;
    int j;
    for (j = 0; j < 4; ++j) {
        LLVMValueRef offset = LLVMBuildAdd(
            builder, base, LLVMConstInt(i32Type, (unsigned)j, false), "off");
        LLVMValueRef indices[2] = { zero, offset };
        ptrs[j] = LLVMBuildInBoundsGEP2(builder, arrayType, buf, indices, 2, "p");
        LLVMBuildStore(
            builder, LLVMConstInt(i32Type, (unsigned)(j * 7 + 1), false), ptrs[j]);
    }
    for (j = 0; j < 4; ++j) {
        LLVMValueRef v = LLVMBuildLoad2(builder, i32Type, ptrs[j], "v");
        sum = LLVMBuildAdd(builder, sum, v, "sum");
    }
    LLVMBuildRet(builder, sum);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    int (*test)(int) = buildFn();
    int base = (int)(intptr_t)opaque((void*)(intptr_t)0);
    int r = test(base);
    /* 1 + 8 + 15 + 22 = 46 */
    if (r != 46) {
        fprintf(stderr, "sum %d, expected 46\n", r);
        return 1;
    }

    printf("safe stack ok\n");
    return 0;
}
