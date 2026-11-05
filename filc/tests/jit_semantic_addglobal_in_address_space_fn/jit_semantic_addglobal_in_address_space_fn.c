#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMAddGlobalInAddressSpace(module, fn-ty, 0). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef fnTy = LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false);
    LLVMAddGlobalInAddressSpace(module, fnTy, "g", 0);
    return 0;
}
