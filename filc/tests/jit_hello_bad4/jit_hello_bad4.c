#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef voidFunctionType = LLVMFunctionType(
        LLVMVoidTypeInContext(context), NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", voidFunctionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef instruction = LLVMBuildUnreachable(builder);
    LLVMValueRef notConstants[2] = { instruction, instruction };

    /* LLVMConstVector demands a llvm::Constant for every element;
       instructions are not constants.  This must panic in the wrapper's
       per-element check instead of confusing LLVM. */
    LLVMValueRef bad = LLVMConstVector(notConstants, 2);

    (void)bad;
    return 0;
}
