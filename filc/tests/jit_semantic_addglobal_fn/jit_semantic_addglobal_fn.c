#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMAddGlobal(module, fn-ty). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef fnTy = LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false);
    LLVMAddGlobal(module, fnTy, "g");
    return 0;
}
