#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildShuffleVector over two <4 x i32> values with a scalar i32 mask
   instead of a vector mask.  The wrapper's operand check rejects it, so this
   is a Fil-C panic (exit 42) instead of the cast<> assertion abort inside
   shuffle-vector construction.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

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
