#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstantPtrAuth with an i32 discriminator.  ConstantPtrAuth::
   ConstantPtrAuth asserts Disc->getBitWidth() == 64 (Constants.cpp); the
   wrapper's operand check must reject it first. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef ptrTy = LLVMPointerTypeInContext(context, 0);
    LLVMConstantPtrAuth(LLVMConstNull(ptrTy), LLVMConstInt(i32, 0, false),
                        LLVMConstInt(i32, 0, false), LLVMConstNull(ptrTy));
    return 0;
}
