#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildAdd(<4 x i32>, i32) mismatch. */

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
    LLVMValueRef slot_va = LLVMBuildAlloca(builder, vec, "slot_va");
    LLVMValueRef va = LLVMBuildLoad2(builder, vec, slot_va, "va");
    LLVMValueRef slot_b = LLVMBuildAlloca(builder, i32, "slot_b");
    LLVMValueRef b = LLVMBuildLoad2(builder, i32, slot_b, "b");
    LLVMBuildAdd(builder, va, b, "r");
    return 0;
}
