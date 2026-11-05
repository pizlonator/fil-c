#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMCreateConstantRangeAttribute(range-kind, 0 bits). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMCreateConstantRangeAttribute(context, 98, 0, NULL, NULL);
    return 0;
}
