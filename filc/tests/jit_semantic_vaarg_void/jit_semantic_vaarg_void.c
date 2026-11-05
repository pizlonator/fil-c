#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildVAArg(ptr, void-ty, "v"). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef ptrTy = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef slot = LLVMBuildAlloca(builder, ptrTy, "slot");
    LLVMBuildVAArg(builder, slot, LLVMVoidTypeInContext(context), "v");
    return 0;
}
