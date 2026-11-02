#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef int32Type = LLVMInt32TypeInContext(context);
    LLVMValueRef notConstantFP = LLVMConstInt(int32Type, 42, false);
    LLVMBool losesInfo = false;

    /* LLVMConstRealGetDouble is a hand-written wrapper, and the LLVM C API
       demands a llvm::ConstantFP.  This must panic in the wrapper instead of
       tripping an LLVM assertion. */
    double result = LLVMConstRealGetDouble(notConstantFP, &losesInfo);

    printf("should not get here: %f\n", result);
    return 0;
}
