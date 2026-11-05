#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMFunctionType(i32, [void], 1). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef params[1];
    params[0] = LLVMVoidTypeInContext(context);
    LLVMFunctionType(i32, params, 1, false);
    return 0;
}
