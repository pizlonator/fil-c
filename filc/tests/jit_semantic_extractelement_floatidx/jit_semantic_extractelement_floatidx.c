#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildExtractElement(<4 x i32>, float-load). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot_n = LLVMBuildAlloca(builder, flt, "slot_n");
    LLVMValueRef n = LLVMBuildLoad2(builder, flt, slot_n, "n");
    LLVMTypeRef vec = LLVMVectorType(i32, 4);
    LLVMValueRef vslot = LLVMBuildAlloca(builder, vec, "vslot");
    LLVMValueRef v = LLVMBuildLoad2(builder, vec, vslot, "v");
    LLVMBuildExtractElement(builder, v, n, "e");
    return 0;
}
