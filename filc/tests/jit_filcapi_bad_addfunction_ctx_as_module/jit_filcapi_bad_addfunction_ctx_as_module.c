#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMAddFunction wants an LLVM module; passing an LLVM context panics
   "object kind mismatch." (exit 42). */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef voidFn =
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false);
    LLVMAddFunction((LLVMModuleRef)context, "f", voidFn);
    return 0;
}
