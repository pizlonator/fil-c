#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildICmp(EQ, float, float). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot_a = LLVMBuildAlloca(builder, flt, "slot_a");
    LLVMValueRef a = LLVMBuildLoad2(builder, flt, slot_a, "a");
    LLVMValueRef slot_b = LLVMBuildAlloca(builder, flt, "slot_b");
    LLVMValueRef b = LLVMBuildLoad2(builder, flt, slot_b, "b");
    LLVMBuildICmp(builder, LLVMIntEQ, a, b, "c");
    return 0;
}
