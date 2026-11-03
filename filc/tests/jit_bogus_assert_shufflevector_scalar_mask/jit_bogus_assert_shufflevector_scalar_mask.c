#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildShuffleVector over two <4 x i32> values with a scalar i32 mask
   instead of a vector mask.  LLVM's shuffle-vector construction aborts
   (SIGABRT) with the cast<> assertion. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef v4 = LLVMVectorType(i32, 4);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef slot = LLVMBuildAlloca(builder, v4, "slot");
    LLVMValueRef v1 = LLVMBuildLoad2(builder, v4, slot, "v1");
    LLVMValueRef v2 = LLVMBuildLoad2(builder, v4, slot, "v2");
    LLVMValueRef mask = LLVMConstInt(i32, 0, false);
    LLVMBuildShuffleVector(builder, v1, v2, mask, "shuf");

    return 0;
}
