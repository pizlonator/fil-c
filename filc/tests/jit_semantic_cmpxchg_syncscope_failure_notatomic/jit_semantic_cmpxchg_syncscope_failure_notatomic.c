#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildAtomicCmpXchgSyncScope with failure ordering=NotAtomic.  The
   sync-scope wrapper shares the cmpxchg checks with LLVMBuildAtomicCmpXchg,
   and AtomicCmpXchgInst::setFailureOrdering asserts "invalid CmpXchg failure
   ordering" (llvm/include/llvm/IR/Instructions.h) for it. */

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
    LLVMBuildAtomicCmpXchgSyncScope(builder, slot, zero, zero,
                                    LLVMAtomicOrderingMonotonic,
                                    LLVMAtomicOrderingNotAtomic, 0);

    return 0;
}
