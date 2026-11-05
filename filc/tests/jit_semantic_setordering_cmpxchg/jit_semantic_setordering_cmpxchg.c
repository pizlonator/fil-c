#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMSetOrdering on a cmpxchg.  Core.cpp's LLVMSetOrdering never dispatches
   to AtomicCmpXchgInst (it would end in cast<StoreInst>), so the wrapper's
   OrderingOwner subclass check rejects the cmpxchg before LLVM ever sees it.
   This pins that the check does not over-accept here rather than leaving the
   input to the cast assert. */

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
    LLVMSetOrdering(cx, LLVMAtomicOrderingMonotonic);

    return 0;
}
