#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstNSWAdd(float, float). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMConstNSWAdd(LLVMConstReal(flt, 1.0), LLVMConstReal(flt, 2.0));
    return 0;
}
