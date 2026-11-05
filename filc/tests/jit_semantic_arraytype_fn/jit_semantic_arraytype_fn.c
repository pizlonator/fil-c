#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMArrayType(fn-ty, 2). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef fnTy = LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false);
    LLVMArrayType(fnTy, 2);
    return 0;
}
