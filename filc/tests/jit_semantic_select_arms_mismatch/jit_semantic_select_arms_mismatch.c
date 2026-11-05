#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildSelect(i1, i32, float) mismatched arms. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMTypeRef i1 = LLVMInt1TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot_a = LLVMBuildAlloca(builder, i32, "slot_a");
    LLVMValueRef a = LLVMBuildLoad2(builder, i32, slot_a, "a");
    LLVMValueRef slot_b = LLVMBuildAlloca(builder, flt, "slot_b");
    LLVMValueRef b = LLVMBuildLoad2(builder, flt, slot_b, "b");
    LLVMBuildSelect(builder, LLVMConstInt(i1, 0, false), a, b, "s");
    return 0;
}
