#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMModuleRef mod = LLVMModuleCreateWithNameInContext("test", ctx);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx);
    LLVMTypeRef fnT = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(mod, "test", fnT);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx, fn, "root");
    LLVMValueRef bbAsValue = LLVMBasicBlockAsValue(entry);

    /* LLVMValueAsMetadata requires a llvm::Constant, llvm::Argument,
       llvm::Instruction, or metadata-as-value; the basic-block-as-value
       bbAsValue is not one.  This must panic in the wrapper instead of tripping
       an LLVM assertion. */
    LLVMValueAsMetadata(bbAsValue);

    return 0;
}
