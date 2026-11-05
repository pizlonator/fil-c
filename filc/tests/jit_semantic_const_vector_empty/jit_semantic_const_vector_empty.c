#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstVector(NULL, 0). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMConstVector(NULL, 0);
    return 0;
}
