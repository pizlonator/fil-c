#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstantPtrAuth with an i64 key.  ConstantPtrAuth::ConstantPtrAuth
   asserts Key->getBitWidth() == 32 (Constants.cpp); the wrapper's operand
   check must reject it first. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMTypeRef ptrTy = LLVMPointerTypeInContext(context, 0);
    LLVMConstantPtrAuth(LLVMConstNull(ptrTy), LLVMConstInt(i64, 0, false),
                        LLVMConstInt(i64, 0, false), LLVMConstNull(ptrTy));
    return 0;
}
