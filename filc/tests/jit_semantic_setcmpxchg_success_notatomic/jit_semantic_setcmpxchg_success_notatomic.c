#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMSetCmpXchgSuccessOrdering with ordering=NotAtomic.  The setter asserts
   "invalid CmpXchg success ordering"
   (llvm/include/llvm/IR/Instructions.h) for it, so the wrapper's
   success-ordering check rejects it before LLVM sees it. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef slot = LLVMBuildAlloca(builder, i32, "slot");
    LLVMValueRef zero = LLVMConstInt(i32, 0, false);
    LLVMValueRef cx = LLVMBuildAtomicCmpXchg(builder, slot, zero, zero,
                                             LLVMAtomicOrderingMonotonic,
                                             LLVMAtomicOrderingMonotonic,
                                             false);
    LLVMSetCmpXchgSuccessOrdering(cx, LLVMAtomicOrderingNotAtomic);

    return 0;
}
