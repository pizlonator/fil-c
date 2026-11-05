#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMSetExact(add). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot_a = LLVMBuildAlloca(builder, i32, "slot_a");
    LLVMValueRef a = LLVMBuildLoad2(builder, i32, slot_a, "a");
    LLVMValueRef slot_b = LLVMBuildAlloca(builder, i32, "slot_b");
    LLVMValueRef b = LLVMBuildLoad2(builder, i32, slot_b, "b");
    LLVMValueRef add = LLVMBuildAdd(builder, a, b, "add");
    LLVMSetExact(add, true);
    return 0;
}
