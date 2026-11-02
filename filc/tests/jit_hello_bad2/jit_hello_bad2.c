#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef int32Type = LLVMInt32TypeInContext(context);
    LLVMValueRef notGlobal = LLVMConstInt(int32Type, 42, false);

    /* LLVMSetLinkage demands a llvm::GlobalValue; a ConstantInt is not one.
       This must panic in the wrapper instead of tripping an LLVM assertion. */
    LLVMSetLinkage(notGlobal, LLVMInternalLinkage);

    return 0;
}
