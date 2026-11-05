#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMConstGEP2 with a floating-point second index.  ConstantExpr::
   getGetElementPtr asserts "GEP indices invalid!"
   (llvm/lib/IR/Constants.cpp) when GetElementPtrInst::getIndexedType rejects
   the indices, and every index after the first must be an integer (the first
   index is exempt: a single-index getelementptr accepts an index of any
   type, and the gep-lie success controls rely on that). */

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
    LLVMConstGEP2(i32, base, idx, 2);

    return 0;
}
