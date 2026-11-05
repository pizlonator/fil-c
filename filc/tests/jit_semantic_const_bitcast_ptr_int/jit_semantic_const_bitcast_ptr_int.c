#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstBitCast(null-ptr-const, i64). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMTypeRef ptrTy = LLVMPointerTypeInContext(context, 0);
    LLVMConstBitCast(LLVMConstNull(ptrTy), i64);
    return 0;
}
