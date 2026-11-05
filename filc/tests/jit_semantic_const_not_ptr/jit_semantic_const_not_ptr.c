#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstNot(null-ptr-const). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef ptrTy = LLVMPointerTypeInContext(context, 0);
    LLVMConstNot(LLVMConstNull(ptrTy));
    return 0;
}
