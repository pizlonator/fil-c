#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMCreateConstantRangeAttribute(range-kind, 128 bits, 7, 7). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    unsigned long long lower[2];
    unsigned long long upper[2];
    lower[0] = 7;
    lower[1] = 0;
    upper[0] = 7;
    upper[1] = 0;
    LLVMCreateConstantRangeAttribute(context, 98, 128, lower, upper);
    return 0;
}
