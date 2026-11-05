#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildAdd(<4 x i32>, <4 x i32>) is legal (vector-splat allowance). */

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
    LLVMValueRef slot_va = LLVMBuildAlloca(builder, vec, "slot_va");
    LLVMValueRef va = LLVMBuildLoad2(builder, vec, slot_va, "va");
    LLVMValueRef slot_vb = LLVMBuildAlloca(builder, vec, "slot_vb");
    LLVMValueRef vb = LLVMBuildLoad2(builder, vec, slot_vb, "vb");
    LLVMBuildAdd(builder, va, vb, "add");
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("add vector ok\n");
    return 0;
}