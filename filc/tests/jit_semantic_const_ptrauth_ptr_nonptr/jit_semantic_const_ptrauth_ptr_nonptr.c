#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstantPtrAuth with a non-pointer ptr.  ConstantPtrAuth::
   ConstantPtrAuth asserts Ptr->getType()->isPointerTy() (Constants.cpp); the
   wrapper's operand check must reject it first. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMConstantPtrAuth(LLVMConstNull(i32), LLVMConstInt(i32, 0, false),
                        LLVMConstInt(LLVMInt64TypeInContext(context), 0, false),
                        LLVMConstNull(i32));
    return 0;
}
