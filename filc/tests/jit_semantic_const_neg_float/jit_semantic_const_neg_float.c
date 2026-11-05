#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstNeg(float-const). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMConstNeg(LLVMConstReal(flt, 1.0));
    return 0;
}
