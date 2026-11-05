#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMConstInBoundsGEP2 with a scalable-vector source element type.  See
   jit_semantic_const_gep_scalable: the three Const-GEP wrappers share the
   source-element-type check. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef sv = LLVMScalableVectorType(i32, 4);
    LLVMTypeRef ptrTy = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef base = LLVMConstNull(ptrTy);

    LLVMValueRef idx[1] = { LLVMConstInt(i32, 0, false) };
    LLVMConstInBoundsGEP2(sv, base, idx, 1);

    return 0;
}
