#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMConstGEPWithNoWrapFlags with a scalable-vector source element type.
   See jit_semantic_const_gep_scalable. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef sv = LLVMScalableVectorType(i32, 4);
    LLVMTypeRef ptrTy = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef base = LLVMConstNull(ptrTy);

    LLVMValueRef idx[1] = { LLVMConstInt(i32, 0, false) };
    LLVMConstGEPWithNoWrapFlags(sv, base, idx, 1, 1 /* in bounds */);

    return 0;
}
