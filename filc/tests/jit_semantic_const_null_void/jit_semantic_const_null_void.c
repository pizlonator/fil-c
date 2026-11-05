#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstNull(void-ty). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMConstNull(LLVMVoidTypeInContext(context));
    return 0;
}
