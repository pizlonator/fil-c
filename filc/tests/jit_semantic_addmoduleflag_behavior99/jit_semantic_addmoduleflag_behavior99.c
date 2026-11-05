#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMAddModuleFlag(module, 99, ...). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMAddModuleFlag(module, 99, "key", 3, LLVMValueAsMetadata(LLVMConstInt(i32, 0, false)));
    return 0;
}
