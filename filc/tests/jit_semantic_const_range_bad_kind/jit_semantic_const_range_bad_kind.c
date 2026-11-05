#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMCreateConstantRangeAttribute(kind=3) is not a range kind. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    unsigned long long lower[1];
    unsigned long long upper[1];
    lower[0] = 10;
    upper[0] = 5;
    LLVMCreateConstantRangeAttribute(context, 3, 32, lower, upper);
    return 0;
}
