#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMConstGEP2 with a scalable-vector source element type.  ConstantExpr::
   getGetElementPtr asserts "Element type is unsupported!"
   (llvm/lib/IR/Constants.cpp) via isSupportedGetElementPtr, which checks the
   type parameter (not the pointee chain) for scalability. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef sv = LLVMScalableVectorType(i32, 4);
    LLVMTypeRef ptrTy = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef base = LLVMConstNull(ptrTy);

    LLVMValueRef idx[1] = { LLVMConstInt(i32, 0, false) };
    LLVMConstGEP2(sv, base, idx, 1);

    return 0;
}
