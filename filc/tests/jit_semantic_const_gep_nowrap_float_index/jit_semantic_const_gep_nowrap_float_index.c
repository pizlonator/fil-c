#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMConstGEPWithNoWrapFlags with a floating-point second index.  See
   jit_semantic_const_gep_float_index. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef f32 = LLVMFloatTypeInContext(context);
    LLVMTypeRef arrayTy = LLVMArrayType(i32, 8);
    LLVMTypeRef ptrTy = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef g = LLVMAddGlobal(module, arrayTy, "g");
    LLVMValueRef base = LLVMConstBitCast(g, ptrTy);

    LLVMValueRef idx[2] = { LLVMConstInt(i32, 0, false),
                            LLVMConstReal(f32, 1.0) };
    LLVMConstGEPWithNoWrapFlags(i32, base, idx, 2, 1 /* in bounds */);

    return 0;
}
