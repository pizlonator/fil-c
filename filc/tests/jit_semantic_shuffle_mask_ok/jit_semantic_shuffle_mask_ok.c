#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildShuffleVector(<4 x i32>, <4 x i32>, <2 x i32> mask). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef vec = LLVMVectorType(i32, 4);
    LLVMValueRef vslot = LLVMBuildAlloca(builder, vec, "vslot");
    LLVMValueRef v1 = LLVMBuildLoad2(builder, vec, vslot, "v1");
    LLVMValueRef v2 = LLVMBuildLoad2(builder, vec, vslot, "v2");
    LLVMValueRef maskElems[2];
    maskElems[0] = LLVMConstInt(i32, 0, false);
    maskElems[1] = LLVMConstInt(i32, 2, false);
    LLVMValueRef mask = LLVMConstVector(maskElems, 2);
    LLVMBuildShuffleVector(builder, v1, v2, mask, "s");
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("shuffle mask ok\n");
    return 0;
}